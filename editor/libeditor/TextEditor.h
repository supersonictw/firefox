/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TextEditor_h
#define mozilla_TextEditor_h

#include "mozilla/EditorBase.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsINamed.h"
#include "nsIPlaintextEditor.h"
#include "nsISupportsImpl.h"
#include "nsITimer.h"
#include "nscore.h"

class nsIContent;
class nsIDocumentEncoder;
class nsIOutputStream;
class nsIPrincipal;
class nsISelectionController;
class nsITransferable;

namespace mozilla {
class AutoEditInitRulesTrigger;
class DeleteNodeTransaction;
class InsertNodeTransaction;
enum class EditSubAction : int32_t;

namespace dom {
class DragEvent;
class Selection;
}  // namespace dom

/**
 * The text editor implementation.
 * Use to edit text document represented as a DOM tree.
 */
class TextEditor : public EditorBase,
                   public nsIPlaintextEditor,
                   public nsITimerCallback,
                   public nsINamed {
 public:
  /****************************************************************************
   * NOTE: DO NOT MAKE YOUR NEW METHODS PUBLIC IF they are called by other
   *       classes under libeditor except EditorEventListener and
   *       HTMLEditorEventListener because each public method which may fire
   *       eEditorInput event will need to instantiate new stack class for
   *       managing input type value of eEditorInput and cache some objects
   *       for smarter handling.  In other words, when you add new root
   *       method to edit the DOM tree, you can make your new method public.
   ****************************************************************************/

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TextEditor, EditorBase)

  TextEditor();

  NS_DECL_NSIPLAINTEXTEDITOR
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  // Overrides of nsIEditor
  NS_IMETHOD GetDocumentIsEmpty(bool* aDocumentIsEmpty) override;

  MOZ_CAN_RUN_SCRIPT
  NS_IMETHOD DeleteSelection(EDirection aAction,
                             EStripWrappers aStripWrappers) override;

  MOZ_CAN_RUN_SCRIPT
  NS_IMETHOD SetDocumentCharacterSet(const nsACString& characterSet) override;

  /**
   * Do "undo" or "redo".
   *
   * @param aCount              How many count of transactions should be
   *                            handled.
   * @param aPrincipal          Set subject principal if it may be called by
   *                            JS.  If set to nullptr, will be treated as
   *                            called by system.
   */
  MOZ_CAN_RUN_SCRIPT nsresult UndoAsAction(uint32_t aCount,
                                           nsIPrincipal* aPrincipal = nullptr);
  MOZ_CAN_RUN_SCRIPT nsresult RedoAsAction(uint32_t aCount,
                                           nsIPrincipal* aPrincipal = nullptr);

  /**
   * Do "cut".
   *
   * @param aPrincipal          If you know current context is subject
   *                            principal or system principal, set it.
   *                            When nullptr, this checks it automatically.
   */
  MOZ_CAN_RUN_SCRIPT nsresult CutAsAction(nsIPrincipal* aPrincipal = nullptr);

  /**
   * IsCutCommandEnabled() returns whether cut command can be enabled or
   * disabled.  This always returns true if we're in non-chrome HTML/XHTML
   * document.  Otherwise, same as the result of `IsCopyToClipboardAllowed()`.
   */
  bool IsCutCommandEnabled() const;

  NS_IMETHOD Copy() override;

  /**
   * IsCopyCommandEnabled() returns copy command can be enabled or disabled.
   * This always returns true if we're in non-chrome HTML/XHTML document.
   * Otherwise, same as the result of `IsCopyToClipboardAllowed()`.
   */
  bool IsCopyCommandEnabled() const;

  /**
   * IsCopyToClipboardAllowed() returns true if the selected content can
   * be copied into the clipboard.  This returns true when:
   * - `Selection` is not collapsed and we're not a password editor.
   * - `Selection` is not collapsed and we're a password editor but selection
   *   range is in unmasked range.
   */
  bool IsCopyToClipboardAllowed() const {
    AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
    if (NS_WARN_IF(!editActionData.CanHandle())) {
      return false;
    }
    return IsCopyToClipboardAllowedInternal();
  }

  /**
   * CanDeleteSelection() returns true if `Selection` is not collapsed and
   * it's allowed to be removed.
   */
  bool CanDeleteSelection() const;

  virtual bool CanPaste(int32_t aClipboardType) const;

  // Shouldn't be used internally, but we need these using declarations for
  // avoiding warnings of clang.
  using EditorBase::CanCopy;
  using EditorBase::CanCut;
  using EditorBase::CanPaste;

  /**
   * Paste aTransferable at Selection.
   *
   * @param aTransferable       Must not be nullptr.
   * @param aPrincipal          Set subject principal if it may be called by
   *                            JS.  If set to nullptr, will be treated as
   *                            called by system.
   */
  MOZ_CAN_RUN_SCRIPT virtual nsresult PasteTransferableAsAction(
      nsITransferable* aTransferable, nsIPrincipal* aPrincipal = nullptr);

  NS_IMETHOD OutputToString(const nsAString& aFormatType, uint32_t aFlags,
                            nsAString& aOutputString) override;

  /** Can we paste |aTransferable| or, if |aTransferable| is null, will a call
   * to pasteTransferable later possibly succeed if given an instance of
   * nsITransferable then? True if the doc is modifiable, and, if
   * |aTransfeable| is non-null, we have pasteable data in |aTransfeable|.
   */
  virtual bool CanPasteTransferable(nsITransferable* aTransferable);

  // Overrides of EditorBase
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult Init(Document& aDoc, Element* aRoot,
                        nsISelectionController* aSelCon, uint32_t aFlags,
                        const nsAString& aValue) override;

  /**
   * IsEmpty() checks whether the editor is empty.  If editor has only padding
   * <br> element for empty editor, returns true.  If editor's root element has
   * non-empty text nodes or other nodes like <br>, returns false.
   */
  nsresult IsEmpty(bool* aIsEmpty) const;
  bool IsEmpty() const {
    bool isEmpty = false;
    nsresult rv = IsEmpty(&isEmpty);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Checking whether the editor is empty failed");
    return NS_SUCCEEDED(rv) && isEmpty;
  }

  MOZ_CAN_RUN_SCRIPT
  virtual nsresult HandleKeyPressEvent(
      WidgetKeyboardEvent* aKeyboardEvent) override;

  virtual dom::EventTarget* GetDOMEventTarget() override;

  /**
   * PasteAsAction() pastes clipboard content to Selection.  This method
   * may dispatch ePaste event first.  If its defaultPrevent() is called,
   * this does nothing but returns NS_OK.
   *
   * @param aClipboardType      nsIClipboard::kGlobalClipboard or
   *                            nsIClipboard::kSelectionClipboard.
   * @param aDispatchPasteEvent true if this should dispatch ePaste event
   *                            before pasting.  Otherwise, false.
   * @param aPrincipal          Set subject principal if it may be called by
   *                            JS.  If set to nullptr, will be treated as
   *                            called by system.
   */
  MOZ_CAN_RUN_SCRIPT nsresult PasteAsAction(int32_t aClipboardType,
                                            bool aDispatchPasteEvent,
                                            nsIPrincipal* aPrincipal = nullptr);

  /**
   * InsertTextAsAction() inserts aStringToInsert at selection.
   * Although this method is implementation of nsIPlaintextEditor.insertText(),
   * this treats the input is an edit action.  If you'd like to insert text
   * as part of edit action, you probably should use InsertTextAsSubAction().
   *
   * @param aStringToInsert     The string to insert.
   * @param aPrincipal          Set subject principal if it may be called by
   *                            JS.  If set to nullptr, will be treated as
   *                            called by system.
   */
  nsresult InsertTextAsAction(const nsAString& aStringToInsert,
                              nsIPrincipal* aPrincipal = nullptr);

  /**
   * PasteAsQuotationAsAction() pastes content in clipboard as quotation.
   * If the editor is TextEditor or in plaintext mode, will paste the content
   * with appending ">" to start of each line.
   *
   * @param aClipboardType      nsIClipboard::kGlobalClipboard or
   *                            nsIClipboard::kSelectionClipboard.
   * @param aDispatchPasteEvent true if this should dispatch ePaste event
   *                            before pasting.  Otherwise, false.
   * @param aPrincipal          Set subject principal if it may be called by
   *                            JS.  If set to nullptr, will be treated as
   *                            called by system.
   */
  MOZ_CAN_RUN_SCRIPT virtual nsresult PasteAsQuotationAsAction(
      int32_t aClipboardType, bool aDispatchPasteEvent,
      nsIPrincipal* aPrincipal = nullptr);

  /**
   * DeleteSelectionAsAction() removes selection content or content around
   * caret with transactions.  This should be used for handling it as an
   * edit action.  If you'd like to remove selection for preparing to insert
   * something, you probably should use DeleteSelectionAsSubAction().
   *
   * @param aDirection          How much range should be removed.
   * @param aStripWrappers      Whether the parent blocks should be removed
   *                            when they become empty.
   * @param aPrincipal          Set subject principal if it may be called by
   *                            JS.  If set to nullptr, will be treated as
   *                            called by system.
   */
  MOZ_CAN_RUN_SCRIPT nsresult
  DeleteSelectionAsAction(EDirection aDirection, EStripWrappers aStripWrappers,
                          nsIPrincipal* aPrincipal = nullptr);

  /**
   * The maximum number of characters allowed.
   *   default: -1 (unlimited).
   */
  int32_t MaxTextLength() const { return mMaxTextLength; }
  void SetMaxTextLength(int32_t aLength) { mMaxTextLength = aLength; }

  /**
   * Replace existed string with a string.
   * This is fast path to replace all string when using single line control.
   *
   * @param aString             The string to be set
   * @param aPrincipal          Set subject principal if it may be called by
   *                            JS.  If set to nullptr, will be treated as
   *                            called by system.
   */
  MOZ_CAN_RUN_SCRIPT nsresult
  SetTextAsAction(const nsAString& aString, nsIPrincipal* aPrincipal = nullptr);

  /**
   * Replace text in aReplaceRange or all text in this editor with aString and
   * treat the change as inserting the string.
   *
   * @param aString             The string to set.
   * @param aReplaceRange       The range to be replaced.
   *                            If nullptr, all contents will be replaced.
   * @param aPrincipal          Set subject principal if it may be called by
   *                            JS.  If set to nullptr, will be treated as
   *                            called by system.
   */
  MOZ_CAN_RUN_SCRIPT nsresult
  ReplaceTextAsAction(const nsAString& aString, nsRange* aReplaceRange,
                      nsIPrincipal* aPrincipal = nullptr);

  /**
   * InsertLineBreakAsAction() is called when user inputs a line break with
   * Enter or something.
   *
   * @param aPrincipal          Set subject principal if it may be called by
   *                            JS.  If set to nullptr, will be treated as
   *                            called by system.
   */
  MOZ_CAN_RUN_SCRIPT virtual nsresult InsertLineBreakAsAction(
      nsIPrincipal* aPrincipal = nullptr);

  /**
   * OnCompositionStart() is called when editor receives eCompositionStart
   * event which should be handled in this editor.
   */
  nsresult OnCompositionStart(WidgetCompositionEvent& aCompositionStartEvent);

  /**
   * OnCompositionChange() is called when editor receives an eCompositioChange
   * event which should be handled in this editor.
   *
   * @param aCompositionChangeEvent     eCompositionChange event which should
   *                                    be handled in this editor.
   */
  MOZ_CAN_RUN_SCRIPT
  nsresult OnCompositionChange(WidgetCompositionEvent& aCompositionChangeEvent);

  /**
   * OnCompositionEnd() is called when editor receives an eCompositionChange
   * event and it's followed by eCompositionEnd event and after
   * OnCompositionChange() is called.
   */
  MOZ_CAN_RUN_SCRIPT
  void OnCompositionEnd(WidgetCompositionEvent& aCompositionEndEvent);

  /**
   * OnDrop() is called from EditorEventListener::Drop that is handler of drop
   * event.
   */
  MOZ_CAN_RUN_SCRIPT
  nsresult OnDrop(dom::DragEvent* aDropEvent);

  /**
   * ComputeTextValue() computes plaintext value of this editor.  This may be
   * too expensive if it's in hot path.
   *
   * @param aDocumentEncoderFlags   Flags of nsIDocumentEncoder.
   * @param aCharset                Encoding of the document.
   */
  nsresult ComputeTextValue(uint32_t aDocumentEncoderFlags,
                            nsAString& aOutputString) const {
    AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
    if (NS_WARN_IF(!editActionData.CanHandle())) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    nsresult rv = ComputeValueInternal(NS_LITERAL_STRING("text/plain"),
                                       aDocumentEncoderFlags, aOutputString);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditorBase::ToGenericNSResult(rv);
    }
    return NS_OK;
  }

  /**
   * Similar to the setter for wrapWidth, but just sets the editor
   * internal state without actually changing the content being edited
   * to wrap at that column.  This should only be used by callers who
   * are sure that their content is already set up correctly.
   */
  void SetWrapColumn(int32_t aWrapColumn) { mWrapColumn = aWrapColumn; }

  /**
   * The following methods are available only when the instance is a password
   * editor.  They return whether there is unmasked range or not and range
   * start and length.
   */
  bool IsAllMasked() const {
    MOZ_ASSERT(IsPasswordEditor());
    return mUnmaskedStart == UINT32_MAX && mUnmaskedLength == 0;
  }
  uint32_t UnmaskedStart() const {
    MOZ_ASSERT(IsPasswordEditor());
    return mUnmaskedStart;
  }
  uint32_t UnmaskedLength() const {
    MOZ_ASSERT(IsPasswordEditor());
    return mUnmaskedLength;
  }
  uint32_t UnmaskedEnd() const {
    MOZ_ASSERT(IsPasswordEditor());
    return mUnmaskedStart + mUnmaskedLength;
  }

  /**
   * IsMaskingPassword() returns false when the last caller of `Unmask()`
   * didn't want to mask again automatically.  When this returns true, user
   * input causes masking the password even before timed-out.
   */
  bool IsMaskingPassword() const {
    MOZ_ASSERT(IsPasswordEditor());
    return mIsMaskingPassword;
  }

  /**
   * PasswordMask() returns a character which masks each character in password
   * fields.
   */
  static char16_t PasswordMask();

 protected:  // May be called by friends.
  /****************************************************************************
   * Some classes like TextEditRules, HTMLEditRules, WSRunObject which are
   * part of handling edit actions are allowed to call the following protected
   * methods.  However, those methods won't prepare caches of some objects
   * which are necessary for them.  So, if you want some following methods
   * to do that for you, you need to create a wrapper method in public scope
   * and call it.
   ****************************************************************************/

  // Overrides of EditorBase
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult RemoveAttributeOrEquivalent(
      Element* aElement, nsAtom* aAttribute,
      bool aSuppressTransaction) override;
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult SetAttributeOrEquivalent(Element* aElement,
                                            nsAtom* aAttribute,
                                            const nsAString& aValue,
                                            bool aSuppressTransaction) override;
  using EditorBase::RemoveAttributeOrEquivalent;
  using EditorBase::SetAttributeOrEquivalent;

  /**
   * InsertTextAsSubAction() inserts aStringToInsert at selection.  This
   * should be used for handling it as an edit sub-action.
   *
   * @param aStringToInsert     The string to insert.
   */
  nsresult InsertTextAsSubAction(const nsAString& aStringToInsert);

  /**
   * DeleteSelectionAsSubAction() removes selection content or content around
   * caret with transactions.  This should be used for handling it as an
   * edit sub-action.
   *
   * @param aDirection          How much range should be removed.
   * @param aStripWrappers      Whether the parent blocks should be removed
   *                            when they become empty.
   */
  MOZ_CAN_RUN_SCRIPT
  nsresult DeleteSelectionAsSubAction(EDirection aDirection,
                                      EStripWrappers aStripWrappers);

  /**
   * DeleteSelectionWithTransaction() removes selected content or content
   * around caret with transactions.
   *
   * @param aDirection          How much range should be removed.
   * @param aStripWrappers      Whether the parent blocks should be removed
   *                            when they become empty.
   */
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult DeleteSelectionWithTransaction(
      EDirection aAction, EStripWrappers aStripWrappers);

  /**
   * Replace existed string with aString.  Caller must guarantee that there
   * is a placeholder transaction which will have the transaction.
   *
   * @ param aString   The string to be set.
   */
  MOZ_CAN_RUN_SCRIPT nsresult SetTextAsSubAction(const nsAString& aString);

  /**
   * ReplaceSelectionAsSubAction() replaces selection with aString.
   *
   * @param aString    The string to replace.
   */
  MOZ_CAN_RUN_SCRIPT
  nsresult ReplaceSelectionAsSubAction(const nsAString& aString);

  /**
   * Extends the selection for given deletion operation
   * If done, also update aAction to what's actually left to do after the
   * extension.
   */
  nsresult ExtendSelectionForDelete(nsIEditor::EDirection* aAction);

  static void GetDefaultEditorPrefs(int32_t& aNewLineHandling,
                                    int32_t& aCaretStyle);

  /**
   * MaybeDoAutoPasswordMasking() may mask password if we're doing auto-masking.
   */
  void MaybeDoAutoPasswordMasking() {
    if (IsPasswordEditor() && IsMaskingPassword()) {
      MaskAllCharacters();
    }
  }

  /**
   * SetUnmaskRange() is available only when the instance is a password
   * editor.  This just updates unmask range.  I.e., caller needs to
   * guarantee to update the layout.
   *
   * @param aStart      First index to show the character.
   *                    If aLength is 0, this value is ignored.
   * @param aLength     Optional, Length to show characters.
   *                    If UINT32_MAX, it means unmasking all characters after
   *                    aStart.
   *                    If 0, it means that masking all characters.
   * @param aTimeout    Optional, specify milliseconds to hide the unmasked
   *                    characters after this call.
   *                    If 0, it means this won't mask the characters
   *                    automatically.
   *                    If aLength is 0, this value is ignored.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult SetUnmaskRange(
      uint32_t aStart, uint32_t aLength = UINT32_MAX, uint32_t aTimeout = 0) {
    return SetUnmaskRangeInternal(aStart, aLength, aTimeout, false, false);
  }

  /**
   * SetUnmaskRangeAndNotify() is available only when the instance is a
   * password editor.  This updates unmask range and notifying the text frame
   * to update the visible characters.
   *
   * @param aStart      First index to show the character.
   *                    If UINT32_MAX, it means masking all.
   * @param aLength     Optional, Length to show characters.
   *                    If UINT32_MAX, it means unmasking all characters after
   *                    aStart.
   * @param aTimeout    Optional, specify milliseconds to hide the unmasked
   *                    characters after this call.
   *                    If 0, it means this won't mask the characters
   *                    automatically.
   *                    If aLength is 0, this value is ignored.
   */
  MOZ_CAN_RUN_SCRIPT nsresult SetUnmaskRangeAndNotify(
      uint32_t aStart, uint32_t aLength = UINT32_MAX, uint32_t aTimeout = 0) {
    return SetUnmaskRangeInternal(aStart, aLength, aTimeout, true, false);
  }

  /**
   * MaskAllCharacters() is an alias of SetUnmaskRange() to mask all characters.
   * In other words, this removes existing unmask range.
   * After this is called, TextEditor starts masking password automatically.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult MaskAllCharacters() {
    return SetUnmaskRangeInternal(UINT32_MAX, 0, 0, false, true);
  }

  /**
   * MaskAllCharactersAndNotify() is an alias of SetUnmaskRangeAndNotify() to
   * mask all characters and notifies the text frame.  In other words, this
   * removes existing unmask range.
   * After this is called, TextEditor starts masking password automatically.
   */
  MOZ_CAN_RUN_SCRIPT nsresult MaskAllCharactersAndNotify() {
    return SetUnmaskRangeInternal(UINT32_MAX, 0, 0, true, true);
  }

  /**
   * WillDeleteText() is called before `DeleteTextTransaction` or something
   * removes text in a text node.  Note that this won't be called if the
   * instance is `HTMLEditor` since supporting it makes the code complicated
   * due to mutation events.
   *
   * @param aCurrentLength      Current text length of the node.
   * @param aRemoveStartOffset  Start offset of the range to be removed.
   * @param aRemoveLength       Length of the range to be removed.
   */
  void WillDeleteText(uint32_t aCurrentLength, uint32_t aRemoveStartOffset,
                      uint32_t aRemoveLength);

  /**
   * DidInsertText() is called after `InsertTextTransaction` or something
   * inserts text into a text node.  Note that this won't be called if the
   * instance is `HTMLEditor` since supporting it makes the code complicated
   * due to mutatione events.
   *
   * @param aNewLength          New text length after the insertion.
   * @param aInsertedOffset     Start offset of the inserted text.
   * @param aInsertedLength     Length of the inserted text.
   * @return                    NS_OK or NS_ERROR_EDITOR_DESTROYED.
   */
  MOZ_CAN_RUN_SCRIPT MOZ_MUST_USE nsresult DidInsertText(
      uint32_t aNewLength, uint32_t aInsertedOffset, uint32_t aInsertedLength);

 protected:  // Called by helper classes.
  virtual void OnStartToHandleTopLevelEditSubAction(
      EditSubAction aEditSubAction, nsIEditor::EDirection aDirection) override;
  MOZ_CAN_RUN_SCRIPT
  virtual void OnEndHandlingTopLevelEditSubAction() override;

  void BeginEditorInit();
  MOZ_CAN_RUN_SCRIPT
  nsresult EndEditorInit();

  /**
   * EnsurePaddingBRElementForEmptyEditor() creates padding <br> element for
   * empty editor or changes padding <br> element for empty last line to for
   * empty editor when we're empty.
   */
  MOZ_CAN_RUN_SCRIPT nsresult EnsurePaddingBRElementForEmptyEditor();

  /**
   * HandleInlineSpellCheckAfterEdit() does spell-check after handling top level
   * edit subaction.
   */
  nsresult HandleInlineSpellCheckAfterEdit() {
    MOZ_ASSERT(IsEditActionDataAvailable());
    if (!GetSpellCheckRestartPoint().IsSet()) {
      return NS_OK;  // Maybe being initialized.
    }
    nsresult rv = HandleInlineSpellCheck(GetSpellCheckRestartPoint());
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to spellcheck");
    ClearSpellCheckRestartPoint();
    return rv;
  }

 protected:  // Shouldn't be used by friend classes
  virtual ~TextEditor();

  int32_t WrapWidth() const { return mWrapColumn; }

  /**
   * Make the given selection span the entire document.
   */
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult SelectEntireDocument() override;

  /**
   * OnInputText() is called when user inputs text with keyboard or something.
   *
   * @param aStringToInsert     The string to insert.
   */
  nsresult OnInputText(const nsAString& aStringToInsert);

  /**
   * InsertLineBreakAsSubAction() inserts a line break, i.e., \n if it's
   * TextEditor or <br> if it's HTMLEditor.
   */
  nsresult InsertLineBreakAsSubAction();

  /**
   * PrepareInsertContent() is a helper method of InsertTextAt(),
   * HTMLEditor::DoInsertHTMLWithContext().  They insert content coming from
   * clipboard or drag and drop.  Before that, they may need to remove selected
   * contents and adjust selection.  This does them instead.
   *
   * @param aPointToInsert      Point to insert.  Must be set.  Callers
   *                            shouldn't use this instance after calling this
   *                            method because this method may cause changing
   *                            the DOM tree and Selection.
   * @param aDoDeleteSelection  true if selected content should be removed.
   */
  MOZ_CAN_RUN_SCRIPT
  nsresult PrepareToInsertContent(const EditorDOMPoint& aPointToInsert,
                                  bool aDoDeleteSelection);

  /**
   * InsertTextAt() inserts aStringToInsert at aPointToInsert.
   *
   * @param aStringToInsert     The string which you want to insert.
   * @param aPointToInsert      The insertion point.
   * @param aDoDeleteSelection  true if you want this to delete selected
   *                            content.  Otherwise, false.
   */
  MOZ_CAN_RUN_SCRIPT
  nsresult InsertTextAt(const nsAString& aStringToInsert,
                        const EditorDOMPoint& aPointToInsert,
                        bool aDoDeleteSelection);

  /**
   * InsertWithQuotationsAsSubAction() inserts aQuotedText with appending ">"
   * to start of every line.
   *
   * @param aQuotedText         String to insert.  This will be quoted by ">"
   *                            automatically.
   */
  nsresult InsertWithQuotationsAsSubAction(const nsAString& aQuotedText);

  /**
   * Return true if the data is safe to insert as the source and destination
   * principals match, or we are in a editor context where this doesn't matter.
   * Otherwise, the data must be sanitized first.
   */
  bool IsSafeToInsertData(Document* aSourceDoc);

  MOZ_CAN_RUN_SCRIPT
  virtual nsresult InitRules();

  /**
   * GetAndInitDocEncoder() returns a document encoder instance for aFormatType
   * after initializing it.  The result may be cached for saving recreation
   * cost.
   *
   * @param aFormatType             MIME type like "text/plain".
   * @param aDocumentEncoderFlags   Flags of nsIDocumentEncoder.
   * @param aCharset                Encoding of the document.
   */
  already_AddRefed<nsIDocumentEncoder> GetAndInitDocEncoder(
      const nsAString& aFormatType, uint32_t aDocumentEncoderFlags,
      const nsACString& aCharset) const;

  /**
   * ComputeValueInternal() computes string value of this editor for given
   * format.  This may be too expensive if it's in hot path.
   *
   * @param aFormatType             MIME type like "text/plain".
   * @param aDocumentEncoderFlags   Flags of nsIDocumentEncoder.
   * @param aCharset                Encoding of the document.
   */
  nsresult ComputeValueInternal(const nsAString& aFormatType,
                                uint32_t aDocumentEncoderFlags,
                                nsAString& aOutputString) const;

  /**
   * Factored methods for handling insertion of data from transferables
   * (drag&drop or clipboard).
   */
  virtual nsresult PrepareTransferable(nsITransferable** transferable);

  nsresult InsertTextFromTransferable(nsITransferable* transferable);

  /**
   * DeleteSelectionAndCreateElement() creates a element whose name is aTag.
   * And insert it into the DOM tree after removing the selected content.
   *
   * @param aTag                The element name to be created.
   * @return                    Created new element.
   */
  MOZ_CAN_RUN_SCRIPT
  already_AddRefed<Element> DeleteSelectionAndCreateElement(nsAtom& aTag);

  /**
   * This method first deletes the selection, if it's not collapsed.  Then if
   * the selection lies in a CharacterData node, it splits it.  If the
   * selection is at this point collapsed in a CharacterData node, it's
   * adjusted to be collapsed right before or after the node instead (which is
   * always possible, since the node was split).
   */
  MOZ_CAN_RUN_SCRIPT nsresult DeleteSelectionAndPrepareToCreateNode();

  /**
   * Shared outputstring; returns whether selection is collapsed and resulting
   * string.
   */
  nsresult SharedOutputString(uint32_t aFlags, bool* aIsCollapsed,
                              nsAString& aResult);

  /**
   * See comment of IsCopyToClipboardAllowed() for the detail.
   */
  bool IsCopyToClipboardAllowedInternal() const;

  bool FireClipboardEvent(EventMessage aEventMessage, int32_t aSelectionType,
                          bool* aActionTaken = nullptr);

  MOZ_CAN_RUN_SCRIPT bool UpdateMetaCharset(Document& aDocument,
                                            const nsACString& aCharacterSet);

  /**
   * EnsureComposition() should be called by composition event handlers.  This
   * tries to get the composition for the event and set it to mComposition.
   * However, this may fail because the composition may be committed before
   * the event comes to the editor.
   *
   * @return            true if there is a composition.  Otherwise, for example,
   *                    a composition event handler in web contents moved focus
   *                    for committing the composition, returns false.
   */
  bool EnsureComposition(WidgetCompositionEvent& aCompositionEvent);

  virtual already_AddRefed<Element> GetInputEventTargetElement() override;

  /**
   * See SetUnmaskRange() and SetUnmaskRangeAndNotify() for the detail.
   *
   * @param aForceStartMasking  If true, forcibly starts masking.  This should
   *                            be used only when `nsIEditor::Mask()` is called.
   */
  MOZ_CAN_RUN_SCRIPT nsresult SetUnmaskRangeInternal(uint32_t aStart,
                                                     uint32_t aLength,
                                                     uint32_t aTimeout,
                                                     bool aNotify,
                                                     bool aForceStartMasking);

 protected:
  mutable nsCOMPtr<nsIDocumentEncoder> mCachedDocumentEncoder;

  // Timer to mask unmasked characters automatically.  Used only when it's
  // a password field.
  nsCOMPtr<nsITimer> mMaskTimer;

  mutable nsString mCachedDocumentEncoderType;

  int32_t mWrapColumn;
  int32_t mMaxTextLength;
  int32_t mInitTriggerCounter;
  int32_t mNewlineHandling;
  int32_t mCaretStyle;

  // Unmasked character range.  Used only when it's a password field.
  // If mUnmaskedLength is 0, it means there is no unmasked characters.
  uint32_t mUnmaskedStart;
  uint32_t mUnmaskedLength;

  // Set to true if all characters are masked or waiting notification from
  // `mMaskTimer`.  Otherwise, i.e., part of or all of password is unmasked
  // without setting `mMaskTimer`, set to false.
  bool mIsMaskingPassword;

  friend class AutoEditInitRulesTrigger;
  friend class DeleteNodeTransaction;
  friend class EditorBase;
  friend class InsertNodeTransaction;
  friend class TextEditRules;
};

}  // namespace mozilla

mozilla::TextEditor* nsIEditor::AsTextEditor() {
  return static_cast<mozilla::TextEditor*>(this);
}

const mozilla::TextEditor* nsIEditor::AsTextEditor() const {
  return static_cast<const mozilla::TextEditor*>(this);
}

#endif  // #ifndef mozilla_TextEditor_h
