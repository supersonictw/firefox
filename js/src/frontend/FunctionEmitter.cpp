/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/FunctionEmitter.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT

#include "builtin/ModuleObject.h"          // ModuleObject
#include "frontend/BytecodeEmitter.h"      // BytecodeEmitter
#include "frontend/ModuleSharedContext.h"  // ModuleSharedContext
#include "frontend/NameAnalysisTypes.h"    // NameLocation
#include "frontend/NameOpEmitter.h"        // NameOpEmitter
#include "frontend/ParseContext.h"         // BindingIter
#include "frontend/PropOpEmitter.h"        // PropOpEmitter
#include "frontend/SharedContext.h"        // SharedContext
#include "vm/AsyncFunction.h"              // AsyncFunctionResolveKind
#include "vm/JSScript.h"                   // JSScript
#include "vm/Opcodes.h"                    // JSOP_*
#include "vm/Scope.h"                      // BindingKind
#include "wasm/AsmJS.h"                    // IsAsmJSModule

using namespace js;
using namespace js::frontend;

using mozilla::Maybe;
using mozilla::Some;

FunctionEmitter::FunctionEmitter(BytecodeEmitter* bce, FunctionBox* funbox,
                                 FunctionSyntaxKind syntaxKind,
                                 IsHoisted isHoisted)
    : bce_(bce),
      funbox_(funbox),
      fun_(bce_->cx, funbox_->function()),
      name_(bce_->cx, fun_->explicitName()),
      syntaxKind_(syntaxKind),
      isHoisted_(isHoisted) {
  MOZ_ASSERT_IF(fun_->isInterpretedLazy(), fun_->lazyScript());
}

bool FunctionEmitter::interpretedCommon() {
  // Mark as singletons any function which will only be executed once, or
  // which is inner to a lambda we only expect to run once. In the latter
  // case, if the lambda runs multiple times then CloneFunctionObject will
  // make a deep clone of its contents.
  bool singleton = bce_->checkRunOnceContext();
  if (!JSFunction::setTypeForScriptedFunction(bce_->cx, fun_, singleton)) {
    return false;
  }

  SharedContext* outersc = bce_->sc;
  if (outersc->isFunctionBox()) {
    outersc->asFunctionBox()->setHasInnerFunctions();
  }

  return true;
}

bool FunctionEmitter::prepareForNonLazy() {
  MOZ_ASSERT(state_ == State::Start);

  MOZ_ASSERT(fun_->isInterpreted());
  MOZ_ASSERT(!fun_->isInterpretedLazy());
  MOZ_ASSERT(!funbox_->wasEmitted);

  //                [stack]

  funbox_->wasEmitted = true;

  if (!interpretedCommon()) {
    return false;
  }

  MOZ_ASSERT_IF(bce_->sc->strict(), funbox_->strictScript);

#ifdef DEBUG
  state_ = State::NonLazy;
#endif
  return true;
}

bool FunctionEmitter::emitNonLazyEnd() {
  MOZ_ASSERT(state_ == State::NonLazy);

  //                [stack]

  if (!emitFunction()) {
    //              [stack] FUN?
    return false;
  }

#ifdef DEBUG
  state_ = State::End;
#endif
  return true;
}

bool FunctionEmitter::emitLazy() {
  MOZ_ASSERT(state_ == State::Start);

  MOZ_ASSERT(fun_->isInterpreted());
  MOZ_ASSERT(fun_->isInterpretedLazy());
  MOZ_ASSERT(!funbox_->wasEmitted);

  //                [stack]

  funbox_->wasEmitted = true;

  if (!interpretedCommon()) {
    return false;
  }

  funbox_->setEnclosingScopeForInnerLazyFunction(bce_->innermostScope());
  if (bce_->emittingRunOnceLambda) {
    // NOTE: The 'funbox' is only partially initialized so we defer checking
    // the shouldSuppressRunOnce condition until delazification.
    fun_->lazyScript()->setTreatAsRunOnce();
  }

  if (!emitFunction()) {
    //              [stack] FUN?
    return false;
  }

#ifdef DEBUG
  state_ = State::End;
#endif
  return true;
}

bool FunctionEmitter::emitAgain() {
  MOZ_ASSERT(state_ == State::Start);
  MOZ_ASSERT(funbox_->wasEmitted);
  MOZ_ASSERT_IF(fun_->hasScript(), fun_->nonLazyScript());

  //                [stack]

  // Annex B block-scoped functions are hoisted like any other assignment
  // that assigns the function to the outer 'var' binding.
  if (!funbox_->isAnnexB) {
#ifdef DEBUG
    state_ = State::End;
#endif
    return true;
  }

  // Get the location of the 'var' binding in the body scope. The
  // name must be found, else there is a bug in the Annex B handling
  // in Parser.
  //
  // In sloppy eval contexts, this location is dynamic.
  Maybe<NameLocation> lhsLoc =
      bce_->locationOfNameBoundInScope(name_, bce_->varEmitterScope);

  // If there are parameter expressions, the var name could be a
  // parameter.
  if (!lhsLoc && bce_->sc->isFunctionBox() &&
      bce_->sc->asFunctionBox()->hasExtraBodyVarScope()) {
    lhsLoc = bce_->locationOfNameBoundInScope(
        name_, bce_->varEmitterScope->enclosingInFrame());
  }

  if (!lhsLoc) {
    lhsLoc = Some(NameLocation::DynamicAnnexBVar());
  } else {
    MOZ_ASSERT(lhsLoc->bindingKind() == BindingKind::Var ||
               lhsLoc->bindingKind() == BindingKind::FormalParameter ||
               (lhsLoc->bindingKind() == BindingKind::Let &&
                bce_->sc->asFunctionBox()->hasParameterExprs));
  }

  NameOpEmitter noe(bce_, name_, *lhsLoc,
                    NameOpEmitter::Kind::SimpleAssignment);
  if (!noe.prepareForRhs()) {
    //              [stack]
    return false;
  }

  if (!bce_->emitGetName(name_)) {
    //              [stack] FUN
    return false;
  }

  if (!noe.emitAssignment()) {
    //              [stack] FUN
    return false;
  }

  if (!bce_->emit1(JSOP_POP)) {
    //              [stack]
    return false;
  }

#ifdef DEBUG
  state_ = State::End;
#endif
  return true;
}

bool FunctionEmitter::emitAsmJSModule() {
  MOZ_ASSERT(state_ == State::Start);

  MOZ_ASSERT(!funbox_->wasEmitted);
  MOZ_ASSERT(IsAsmJSModule(fun_));

  //                [stack]

  funbox_->wasEmitted = true;

  if (!emitFunction()) {
    //              [stack]
    return false;
  }

#ifdef DEBUG
  state_ = State::End;
#endif
  return true;
}

bool FunctionEmitter::emitFunction() {
  // Make the function object a literal in the outer script's pool.
  uint32_t index;
  if (!bce_->perScriptData().gcThingList().append(funbox_, &index)) {
    return false;
  }

  //                [stack]

  if (isHoisted_ == IsHoisted::No) {
    return emitNonHoisted(index);
    //              [stack] FUN?
  }

  bool topLevelFunction;
  if (bce_->sc->isFunctionBox() ||
      (bce_->sc->isEvalContext() && bce_->sc->strict())) {
    // No nested functions inside other functions are top-level.
    topLevelFunction = false;
  } else {
    // In sloppy eval scripts, top-level functions are accessed dynamically.
    // In global and module scripts, top-level functions are those bound in
    // the var scope.
    NameLocation loc = bce_->lookupName(name_);
    topLevelFunction = loc.kind() == NameLocation::Kind::Dynamic ||
                       loc.bindingKind() == BindingKind::Var;
  }

  if (topLevelFunction) {
    return emitTopLevelFunction(index);
    //              [stack]
  }

  return emitHoisted(index);
  //                [stack]
}

bool FunctionEmitter::emitNonHoisted(unsigned index) {
  // Non-hoisted functions simply emit their respective op.

  //                [stack]

  // JSOP_LAMBDA_ARROW is always preceded by a opcode that pushes new.target.
  // See below.
  MOZ_ASSERT(fun_->isArrow() == (syntaxKind_ == FunctionSyntaxKind::Arrow));

  if (fun_->isArrow()) {
    if (!emitNewTargetForArrow()) {
      //            [stack] NEW.TARGET/NULL
      return false;
    }
  }

  if (syntaxKind_ == FunctionSyntaxKind::DerivedClassConstructor) {
    //              [stack] PROTO
    if (!bce_->emitIndex32(JSOP_FUNWITHPROTO, index)) {
      //            [stack] FUN
      return false;
    }
    return true;
  }

  // This is a FunctionExpression, ArrowFunctionExpression, or class
  // constructor. Emit the single instruction (without location info).
  JSOp op = syntaxKind_ == FunctionSyntaxKind::Arrow ? JSOP_LAMBDA_ARROW
                                                     : JSOP_LAMBDA;
  if (!bce_->emitIndex32(op, index)) {
    //              [stack] FUN
    return false;
  }

  return true;
}

bool FunctionEmitter::emitHoisted(unsigned index) {
  MOZ_ASSERT(syntaxKind_ == FunctionSyntaxKind::Statement);

  //                [stack]

  // For functions nested within functions and blocks, make a lambda and
  // initialize the binding name of the function in the current scope.

  NameOpEmitter noe(bce_, name_, NameOpEmitter::Kind::Initialize);
  if (!noe.prepareForRhs()) {
    //              [stack]
    return false;
  }

  if (!bce_->emitIndexOp(JSOP_LAMBDA, index)) {
    //              [stack] FUN
    return false;
  }

  if (!noe.emitAssignment()) {
    //              [stack] FUN
    return false;
  }

  if (!bce_->emit1(JSOP_POP)) {
    //              [stack]
    return false;
  }

  return true;
}

bool FunctionEmitter::emitTopLevelFunction(unsigned index) {
  //                [stack]

  if (bce_->sc->isModuleContext()) {
    // For modules, we record the function and instantiate the binding
    // during ModuleInstantiate(), before the script is run.

    JS::Rooted<ModuleObject*> module(bce_->cx,
                                     bce_->sc->asModuleContext()->module());
    if (!module->noteFunctionDeclaration(bce_->cx, name_, fun_)) {
      return false;
    }
    return true;
  }

  MOZ_ASSERT(bce_->sc->isGlobalContext() || bce_->sc->isEvalContext());
  MOZ_ASSERT(syntaxKind_ == FunctionSyntaxKind::Statement);
  MOZ_ASSERT(bce_->inPrologue());

  if (!bce_->emitIndex32(JSOP_LAMBDA, index)) {
    //              [stack] FUN
    return false;
  }
  if (!bce_->emit1(JSOP_DEFFUN)) {
    //              [stack]
    return false;
  }
  return true;
}

bool FunctionEmitter::emitNewTargetForArrow() {
  //                [stack]

  if (bce_->sc->allowNewTarget()) {
    if (!bce_->emit1(JSOP_NEWTARGET)) {
      //            [stack] NEW.TARGET
      return false;
    }
  } else {
    if (!bce_->emit1(JSOP_NULL)) {
      //            [stack] NULL
      return false;
    }
  }

  return true;
}

bool FunctionScriptEmitter::prepareForParameters() {
  MOZ_ASSERT(state_ == State::Start);
  MOZ_ASSERT(bce_->inPrologue());

  //                [stack]

  if (paramStart_) {
    bce_->setScriptStartOffsetIfUnset(*paramStart_);
  }

  // The ordering of these EmitterScopes is important. The named lambda
  // scope needs to enclose the function scope needs to enclose the extra
  // var scope.

  if (funbox_->namedLambdaBindings()) {
    namedLambdaEmitterScope_.emplace(bce_);
    if (!namedLambdaEmitterScope_->enterNamedLambda(bce_, funbox_)) {
      return false;
    }
  }

  /*
   * Mark the script so that initializers created within it may be given more
   * precise types.
   */
  if (bce_->isRunOnceLambda()) {
    bce_->script->setTreatAsRunOnce();
    MOZ_ASSERT(!bce_->script->hasRunOnce());
  }

  if (bodyEnd_) {
    bce_->setFunctionBodyEndPos(*bodyEnd_);
  }

  if (paramStart_) {
    if (!bce_->updateLineNumberNotes(*paramStart_)) {
      return false;
    }
  }

  tdzCache_.emplace(bce_);
  functionEmitterScope_.emplace(bce_);

  if (funbox_->hasParameterExprs) {
    // There's parameter exprs, emit them in the main section.
    //
    // One caveat is that Debugger considers ops in the prologue to be
    // unreachable (i.e. cannot set a breakpoint on it). If there are no
    // parameter exprs, any unobservable environment ops (like pushing the
    // call object, setting '.this', etc) need to go in the prologue, else it
    // messes up breakpoint tests.
    if (!bce_->switchToMain()) {
      return false;
    }
  }

  if (!functionEmitterScope_->enterFunction(bce_, funbox_)) {
    return false;
  }

  if (!bce_->emitInitializeFunctionSpecialNames()) {
    //              [stack]
    return false;
  }

  if (!funbox_->hasParameterExprs) {
    if (!bce_->switchToMain()) {
      return false;
    }
  }

  // Parameters can't reuse the reject try-catch block from the function body,
  // because the body may have pushed an additional var-environment. This
  // messes up scope resolution for the |.generator| variable, because we'd
  // need different hops to reach |.generator| depending on whether the error
  // was thrown from the parameters or the function body.
  if (funbox_->hasParameterExprs && funbox_->needsPromiseResult()) {
    if (!emitAsyncFunctionRejectPrologue()) {
      return false;
    }
  }

#ifdef DEBUG
  state_ = State::Parameters;
#endif
  return true;
}

bool FunctionScriptEmitter::prepareForBody() {
  MOZ_ASSERT(state_ == State::Parameters);

  //                [stack]

  if (rejectTryCatch_) {
    if (!emitAsyncFunctionRejectEpilogue()) {
      return false;
    }
  }

  if (!emitExtraBodyVarScope()) {
    //              [stack]
    return false;
  }

  if (funbox_->needsPromiseResult()) {
    if (!emitAsyncFunctionRejectPrologue()) {
      return false;
    }
  }

  if (funbox_->kind() == FunctionFlags::FunctionKind::ClassConstructor) {
    if (!funbox_->isDerivedClassConstructor()) {
      if (!bce_->emitInitializeInstanceFields()) {
        //          [stack]
        return false;
      }
    }
  }

#ifdef DEBUG
  state_ = State::Body;
#endif
  return true;
}

bool FunctionScriptEmitter::emitAsyncFunctionRejectPrologue() {
  rejectTryCatch_.emplace(bce_, TryEmitter::Kind::TryCatch,
                          TryEmitter::ControlKind::NonSyntactic);
  return rejectTryCatch_->emitTry();
}

bool FunctionScriptEmitter::emitAsyncFunctionRejectEpilogue() {
  if (!rejectTryCatch_->emitCatch()) {
    //              [stack] EXC
    return false;
  }

  if (!bce_->emitGetDotGeneratorInInnermostScope()) {
    //              [stack] EXC GEN
    return false;
  }
  if (!bce_->emit2(JSOP_ASYNCRESOLVE,
                   uint8_t(AsyncFunctionResolveKind::Reject))) {
    //              [stack] PROMISE
    return false;
  }
  if (!bce_->emit1(JSOP_SETRVAL)) {
    //              [stack]
    return false;
  }
  if (!bce_->emitGetDotGeneratorInInnermostScope()) {
    //              [stack] GEN
    return false;
  }
  if (!bce_->emitYieldOp(JSOP_FINALYIELDRVAL)) {
    //              [stack]
    return false;
  }

  if (!rejectTryCatch_->emitEnd()) {
    return false;
  }
  rejectTryCatch_.reset();
  return true;
}

bool FunctionScriptEmitter::emitExtraBodyVarScope() {
  //                [stack]

  if (!funbox_->hasExtraBodyVarScope()) {
    return true;
  }

  extraBodyVarEmitterScope_.emplace(bce_);
  if (!extraBodyVarEmitterScope_->enterFunctionExtraBodyVar(bce_, funbox_)) {
    return false;
  }

  if (!funbox_->extraVarScopeBindings() || !funbox_->functionScopeBindings()) {
    return true;
  }

  // After emitting expressions for all parameters, copy over any formal
  // parameters which have been redeclared as vars. For example, in the
  // following, the var y in the body scope is 42:
  //
  //   function f(x, y = 42) { var y; }
  //
  JS::Rooted<JSAtom*> name(bce_->cx);
  for (BindingIter bi(*funbox_->functionScopeBindings(), true); bi; bi++) {
    name = bi.name();

    // There may not be a var binding of the same name.
    if (!bce_->locationOfNameBoundInScope(name,
                                          extraBodyVarEmitterScope_.ptr())) {
      continue;
    }

    // The '.this' and '.generator' function special
    // bindings should never appear in the extra var
    // scope. 'arguments', however, may.
    MOZ_ASSERT(name != bce_->cx->names().dotThis &&
               name != bce_->cx->names().dotGenerator);

    NameOpEmitter noe(bce_, name, NameOpEmitter::Kind::Initialize);
    if (!noe.prepareForRhs()) {
      //            [stack]
      return false;
    }

    NameLocation paramLoc =
        *bce_->locationOfNameBoundInScope(name, functionEmitterScope_.ptr());
    if (!bce_->emitGetNameAtLocation(name, paramLoc)) {
      //            [stack] VAL
      return false;
    }

    if (!noe.emitAssignment()) {
      //            [stack] VAL
      return false;
    }

    if (!bce_->emit1(JSOP_POP)) {
      //            [stack]
      return false;
    }
  }

  return true;
}

bool FunctionScriptEmitter::emitEndBody() {
  MOZ_ASSERT(state_ == State::Body);

  //                [stack]

  if (funbox_->needsFinalYield()) {
    // If we fall off the end of a generator, do a final yield.
    bool needsIteratorResult = funbox_->needsIteratorResult();
    if (needsIteratorResult) {
      if (!bce_->emitPrepareIteratorResult()) {
        //          [stack] RESULT
        return false;
      }
    }

    if (!bce_->emit1(JSOP_UNDEFINED)) {
      //            [stack] RESULT? UNDEF
      return false;
    }

    if (needsIteratorResult) {
      if (!bce_->emitFinishIteratorResult(true)) {
        //          [stack] RESULT
        return false;
      }
    }

    if (funbox_->needsPromiseResult()) {
      if (!bce_->emitGetDotGeneratorInInnermostScope()) {
        //          [stack] RVAL GEN
        return false;
      }

      if (!bce_->emit2(JSOP_ASYNCRESOLVE,
                       uint8_t(AsyncFunctionResolveKind::Fulfill))) {
        //          [stack] PROMISE
        return false;
      }
    }

    if (!bce_->emit1(JSOP_SETRVAL)) {
      //            [stack]
      return false;
    }

    if (!bce_->emitGetDotGeneratorInInnermostScope()) {
      //            [stack] GEN
      return false;
    }

    // No need to check for finally blocks, etc as in EmitReturn.
    if (!bce_->emitYieldOp(JSOP_FINALYIELDRVAL)) {
      //            [stack]
      return false;
    }
  } else {
    // Non-generator functions just return |undefined|. The
    // JSOP_RETRVAL emitted below will do that, except if the
    // script has a finally block: there can be a non-undefined
    // value in the return value slot. Make sure the return value
    // is |undefined|.
    if (bce_->hasTryFinally) {
      if (!bce_->emit1(JSOP_UNDEFINED)) {
        //          [stack] UNDEF
        return false;
      }
      if (!bce_->emit1(JSOP_SETRVAL)) {
        //          [stack]
        return false;
      }
    }
  }

  if (funbox_->isDerivedClassConstructor()) {
    if (!bce_->emitCheckDerivedClassConstructorReturn()) {
      //            [stack]
      return false;
    }
  }

  if (rejectTryCatch_) {
    if (!emitAsyncFunctionRejectEpilogue()) {
      return false;
    }
  }

  if (extraBodyVarEmitterScope_) {
    if (!extraBodyVarEmitterScope_->leave(bce_)) {
      return false;
    }

    extraBodyVarEmitterScope_.reset();
  }

  if (!functionEmitterScope_->leave(bce_)) {
    return false;
  }
  functionEmitterScope_.reset();
  tdzCache_.reset();

  if (bodyEnd_) {
    if (!bce_->updateSourceCoordNotes(*bodyEnd_)) {
      return false;
    }
  }

  // We only want to mark the end of a function as a breakable position if
  // there is token there that the user can easily associate with the function
  // as a whole. Since arrow function single-expression bodies have no closing
  // curly bracket, we do not place a breakpoint at their end position.
  if (!funbox_->hasExprBody()) {
    if (!bce_->markSimpleBreakpoint()) {
      return false;
    }
  }

  // Always end the script with a JSOP_RETRVAL. Some other parts of the
  // codebase depend on this opcode,
  // e.g. InterpreterRegs::setToEndOfScript.
  if (!bce_->emitReturnRval()) {
    //              [stack]
    return false;
  }

  if (namedLambdaEmitterScope_) {
    if (!namedLambdaEmitterScope_->leave(bce_)) {
      return false;
    }
    namedLambdaEmitterScope_.reset();
  }

#ifdef DEBUG
  state_ = State::EndBody;
#endif
  return true;
}

bool FunctionScriptEmitter::initScript() {
  MOZ_ASSERT(state_ == State::EndBody);

  if (!JSScript::fullyInitFromEmitter(bce_->cx, bce_->script, bce_)) {
    return false;
  }

  bce_->tellDebuggerAboutCompiledScript(bce_->cx);

#ifdef DEBUG
  state_ = State::End;
#endif
  return true;
}

FunctionParamsEmitter::FunctionParamsEmitter(BytecodeEmitter* bce,
                                             FunctionBox* funbox)
    : bce_(bce),
      funbox_(funbox),
      functionEmitterScope_(bce_->innermostEmitterScope()) {}

bool FunctionParamsEmitter::emitSimple(JS::Handle<JSAtom*> paramName) {
  MOZ_ASSERT(state_ == State::Start);

  //                [stack]

  if (funbox_->hasParameterExprs) {
    if (!bce_->emitArgOp(JSOP_GETARG, argSlot_)) {
      //            [stack] ARG
      return false;
    }

    if (!emitAssignment(paramName)) {
      //            [stack]
      return false;
    }
  }

  argSlot_++;
  return true;
}

bool FunctionParamsEmitter::prepareForDefault() {
  MOZ_ASSERT(state_ == State::Start);

  //                [stack]

  if (!enterParameterExpressionVarScope()) {
    return false;
  }

  if (!prepareForInitializer()) {
    //              [stack]
    return false;
  }

#ifdef DEBUG
  state_ = State::Default;
#endif
  return true;
}

bool FunctionParamsEmitter::emitDefaultEnd(JS::Handle<JSAtom*> paramName) {
  MOZ_ASSERT(state_ == State::Default);

  //                [stack] DEFAULT

  if (!emitInitializerEnd()) {
    //              [stack] ARG/DEFAULT
    return false;
  }
  if (!emitAssignment(paramName)) {
    //              [stack]
    return false;
  }
  if (!leaveParameterExpressionVarScope()) {
    return false;
  }

  argSlot_++;

#ifdef DEBUG
  state_ = State::Start;
#endif
  return true;
}

bool FunctionParamsEmitter::prepareForDestructuring() {
  MOZ_ASSERT(state_ == State::Start);

  //                [stack]

  if (!enterParameterExpressionVarScope()) {
    return false;
  }

  if (!bce_->emitArgOp(JSOP_GETARG, argSlot_)) {
    //              [stack] ARG
    return false;
  }

#ifdef DEBUG
  state_ = State::Destructuring;
#endif
  return true;
}

bool FunctionParamsEmitter::emitDestructuringEnd() {
  MOZ_ASSERT(state_ == State::Destructuring);

  //                [stack] ARG

  if (!bce_->emit1(JSOP_POP)) {
    //              [stack]
    return false;
  }

  if (!leaveParameterExpressionVarScope()) {
    return false;
  }

  argSlot_++;

#ifdef DEBUG
  state_ = State::Start;
#endif
  return true;
}

bool FunctionParamsEmitter::prepareForDestructuringDefaultInitializer() {
  MOZ_ASSERT(state_ == State::Start);

  //                [stack]

  if (!enterParameterExpressionVarScope()) {
    return false;
  }
  if (!prepareForInitializer()) {
    //              [stack]
    return false;
  }

#ifdef DEBUG
  state_ = State::DestructuringDefaultInitializer;
#endif
  return true;
}

bool FunctionParamsEmitter::prepareForDestructuringDefault() {
  MOZ_ASSERT(state_ == State::DestructuringDefaultInitializer);

  //                [stack] DEFAULT

  if (!emitInitializerEnd()) {
    //              [stack] ARG/DEFAULT
    return false;
  }

#ifdef DEBUG
  state_ = State::DestructuringDefault;
#endif
  return true;
}

bool FunctionParamsEmitter::emitDestructuringDefaultEnd() {
  MOZ_ASSERT(state_ == State::DestructuringDefault);

  //                [stack] ARG/DEFAULT

  if (!bce_->emit1(JSOP_POP)) {
    //              [stack]
    return false;
  }

  if (!leaveParameterExpressionVarScope()) {
    return false;
  }

  argSlot_++;

#ifdef DEBUG
  state_ = State::Start;
#endif
  return true;
}

bool FunctionParamsEmitter::emitRest(JS::Handle<JSAtom*> paramName) {
  MOZ_ASSERT(state_ == State::Start);

  //                [stack]

  if (!emitRestArray()) {
    //              [stack] REST
    return false;
  }
  if (!emitAssignment(paramName)) {
    //              [stack]
    return false;
  }

#ifdef DEBUG
  state_ = State::End;
#endif
  return true;
}

bool FunctionParamsEmitter::prepareForDestructuringRest() {
  MOZ_ASSERT(state_ == State::Start);

  //                [stack]

  if (!enterParameterExpressionVarScope()) {
    return false;
  }
  if (!emitRestArray()) {
    //              [stack] REST
    return false;
  }

#ifdef DEBUG
  state_ = State::DestructuringRest;
#endif
  return true;
}

bool FunctionParamsEmitter::emitDestructuringRestEnd() {
  MOZ_ASSERT(state_ == State::DestructuringRest);

  //                [stack] REST

  if (!bce_->emit1(JSOP_POP)) {
    //              [stack]
    return false;
  }

  if (!leaveParameterExpressionVarScope()) {
    return false;
  }

#ifdef DEBUG
  state_ = State::End;
#endif
  return true;
}

bool FunctionParamsEmitter::enterParameterExpressionVarScope() {
  if (!funbox_->hasDirectEvalInParameterExpr) {
    return true;
  }

  // ES 14.1.19 says if BindingElement contains an expression in the
  // production FormalParameter : BindingElement, it is evaluated in a
  // new var environment. This is needed to prevent vars from escaping
  // direct eval in parameter expressions.
  paramExprVarEmitterScope_.emplace(bce_);
  if (!paramExprVarEmitterScope_->enterParameterExpressionVar(bce_)) {
    return false;
  }
  return true;
}

bool FunctionParamsEmitter::leaveParameterExpressionVarScope() {
  if (!paramExprVarEmitterScope_) {
    return true;
  }

  if (!paramExprVarEmitterScope_->leave(bce_)) {
    return false;
  }
  paramExprVarEmitterScope_.reset();

  return true;
}

bool FunctionParamsEmitter::prepareForInitializer() {
  //                [stack]

  // If we have an initializer, emit the initializer and assign it
  // to the argument slot. TDZ is taken care of afterwards.
  MOZ_ASSERT(funbox_->hasParameterExprs);
  if (!bce_->emitArgOp(JSOP_GETARG, argSlot_)) {
    //              [stack] ARG
    return false;
  }
  default_.emplace(bce_);
  if (!default_->prepareForDefault()) {
    //              [stack]
    return false;
  }
  return true;
}

bool FunctionParamsEmitter::emitInitializerEnd() {
  //                [stack] DEFAULT

  if (!default_->emitEnd()) {
    //              [stack] ARG/DEFAULT
    return false;
  }
  default_.reset();
  return true;
}

bool FunctionParamsEmitter::emitRestArray() {
  //                [stack]

  if (!bce_->emit1(JSOP_REST)) {
    //              [stack] REST
    return false;
  }
  return true;
}

bool FunctionParamsEmitter::emitAssignment(JS::Handle<JSAtom*> paramName) {
  //                [stack] ARG

  NameLocation paramLoc =
      *bce_->locationOfNameBoundInScope(paramName, functionEmitterScope_);

  // RHS is already pushed in the caller side.
  // Make sure prepareForRhs doesn't touch stack.
  MOZ_ASSERT(paramLoc.kind() == NameLocation::Kind::ArgumentSlot ||
             paramLoc.kind() == NameLocation::Kind::FrameSlot ||
             paramLoc.kind() == NameLocation::Kind::EnvironmentCoordinate);

  NameOpEmitter noe(bce_, paramName, paramLoc, NameOpEmitter::Kind::Initialize);
  if (!noe.prepareForRhs()) {
    //              [stack] ARG
    return false;
  }

  if (!noe.emitAssignment()) {
    //              [stack] ARG
    return false;
  }

  if (!bce_->emit1(JSOP_POP)) {
    //              [stack]
    return false;
  }

  return true;
}

DestructuringFlavor FunctionParamsEmitter::getDestructuringFlavor() {
  MOZ_ASSERT(state_ == State::Destructuring ||
             state_ == State::DestructuringDefault ||
             state_ == State::DestructuringRest);

  return funbox_->hasDirectEvalInParameterExpr
             ? DestructuringFlavor::FormalParameterInVarScope
             : DestructuringFlavor::Declaration;
}
