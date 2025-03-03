/* Copyright 2018 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

use core::result;
use std::boxed::Box;
use std::fmt;

#[cfg(feature = "std")]
use std::error::Error;

#[derive(Debug, Copy, Clone)]
pub struct BinaryReaderError {
    pub message: &'static str,
    pub offset: usize,
}

pub type Result<T> = result::Result<T, BinaryReaderError>;

#[cfg(feature = "std")]
impl Error for BinaryReaderError {}

impl fmt::Display for BinaryReaderError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{} (at offset {})", self.message, self.offset)
    }
}

#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub enum CustomSectionKind {
    Unknown,
    Name,
    Producers,
    SourceMappingURL,
    Reloc,
    Linking,
}

/// Section code as defined [here].
///
/// [here]: https://webassembly.github.io/spec/core/binary/modules.html#sections
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub enum SectionCode<'a> {
    Custom {
        name: &'a str,
        kind: CustomSectionKind,
    },
    Type,      // Function signature declarations
    Import,    // Import declarations
    Function,  // Function declarations
    Table,     // Indirect function table and other tables
    Memory,    // Memory attributes
    Global,    // Global declarations
    Export,    // Exports
    Start,     // Start function declaration
    Element,   // Elements section
    Code,      // Function bodies (code)
    Data,      // Data segments
    DataCount, // Count of passive data segments
}

/// Types as defined [here].
///
/// [here]: https://webassembly.github.io/spec/core/syntax/types.html#types
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum Type {
    I32,
    I64,
    F32,
    F64,
    V128,
    AnyFunc,
    AnyRef,
    Func,
    EmptyBlockType,
}

/// Either a value type or a function type.
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum TypeOrFuncType {
    /// A value type.
    ///
    /// When used as the type for a block, this type is the optional result
    /// type: `[] -> [t?]`.
    Type(Type),

    /// A function type (referenced as an index into the types section).
    FuncType(u32),
}

/// External types as defined [here].
///
/// [here]: https://webassembly.github.io/spec/core/syntax/types.html#external-types
#[derive(Debug, Copy, Clone)]
pub enum ExternalKind {
    Function,
    Table,
    Memory,
    Global,
}

#[derive(Debug, Clone)]
pub struct FuncType {
    pub form: Type,
    pub params: Box<[Type]>,
    pub returns: Box<[Type]>,
}

#[derive(Debug, Copy, Clone)]
pub struct ResizableLimits {
    pub initial: u32,
    pub maximum: Option<u32>,
}

#[derive(Debug, Copy, Clone)]
pub struct TableType {
    pub element_type: Type,
    pub limits: ResizableLimits,
}

#[derive(Debug, Copy, Clone)]
pub struct MemoryType {
    pub limits: ResizableLimits,
    pub shared: bool,
}

#[derive(Debug, Copy, Clone)]
pub struct GlobalType {
    pub content_type: Type,
    pub mutable: bool,
}

#[derive(Debug, Copy, Clone)]
pub enum ImportSectionEntryType {
    Function(u32),
    Table(TableType),
    Memory(MemoryType),
    Global(GlobalType),
}

#[derive(Debug)]
pub struct MemoryImmediate {
    pub flags: u32,
    pub offset: u32,
}

#[derive(Debug, Copy, Clone)]
pub struct Naming<'a> {
    pub index: u32,
    pub name: &'a str,
}

#[derive(Debug, Copy, Clone)]
pub enum NameType {
    Module,
    Function,
    Local,
}

#[derive(Debug, Copy, Clone)]
pub enum LinkingType {
    StackPointer(u32),
}

#[derive(Debug, Copy, Clone)]
pub enum RelocType {
    FunctionIndexLEB,
    TableIndexSLEB,
    TableIndexI32,
    GlobalAddrLEB,
    GlobalAddrSLEB,
    GlobalAddrI32,
    TypeIndexLEB,
    GlobalIndexLEB,
}

/// A br_table entries representation.
#[derive(Debug)]
pub struct BrTable<'a> {
    pub(crate) buffer: &'a [u8],
    pub(crate) cnt: usize,
}

/// An IEEE binary32 immediate floating point value, represented as a u32
/// containing the bitpattern.
///
/// All bit patterns are allowed.
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub struct Ieee32(pub(crate) u32);

impl Ieee32 {
    pub fn bits(self) -> u32 {
        self.0
    }
}

/// An IEEE binary64 immediate floating point value, represented as a u64
/// containing the bitpattern.
///
/// All bit patterns are allowed.
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub struct Ieee64(pub(crate) u64);

impl Ieee64 {
    pub fn bits(self) -> u64 {
        self.0
    }
}

#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub struct V128(pub(crate) [u8; 16]);

impl V128 {
    pub fn bytes(&self) -> &[u8; 16] {
        &self.0
    }
}

pub type SIMDLaneIndex = u8;

/// Instructions as defined [here].
///
/// [here]: https://webassembly.github.io/spec/core/binary/instructions.html
#[derive(Debug)]
pub enum Operator<'a> {
    Unreachable,
    Nop,
    Block { ty: TypeOrFuncType },
    Loop { ty: TypeOrFuncType },
    If { ty: TypeOrFuncType },
    Else,
    End,
    Br { relative_depth: u32 },
    BrIf { relative_depth: u32 },
    BrTable { table: BrTable<'a> },
    Return,
    Call { function_index: u32 },
    CallIndirect { index: u32, table_index: u32 },
    Drop,
    Select,
    GetLocal { local_index: u32 },
    SetLocal { local_index: u32 },
    TeeLocal { local_index: u32 },
    GetGlobal { global_index: u32 },
    SetGlobal { global_index: u32 },
    I32Load { memarg: MemoryImmediate },
    I64Load { memarg: MemoryImmediate },
    F32Load { memarg: MemoryImmediate },
    F64Load { memarg: MemoryImmediate },
    I32Load8S { memarg: MemoryImmediate },
    I32Load8U { memarg: MemoryImmediate },
    I32Load16S { memarg: MemoryImmediate },
    I32Load16U { memarg: MemoryImmediate },
    I64Load8S { memarg: MemoryImmediate },
    I64Load8U { memarg: MemoryImmediate },
    I64Load16S { memarg: MemoryImmediate },
    I64Load16U { memarg: MemoryImmediate },
    I64Load32S { memarg: MemoryImmediate },
    I64Load32U { memarg: MemoryImmediate },
    I32Store { memarg: MemoryImmediate },
    I64Store { memarg: MemoryImmediate },
    F32Store { memarg: MemoryImmediate },
    F64Store { memarg: MemoryImmediate },
    I32Store8 { memarg: MemoryImmediate },
    I32Store16 { memarg: MemoryImmediate },
    I64Store8 { memarg: MemoryImmediate },
    I64Store16 { memarg: MemoryImmediate },
    I64Store32 { memarg: MemoryImmediate },
    MemorySize { reserved: u32 },
    MemoryGrow { reserved: u32 },
    I32Const { value: i32 },
    I64Const { value: i64 },
    F32Const { value: Ieee32 },
    F64Const { value: Ieee64 },
    RefNull,
    RefIsNull,
    I32Eqz,
    I32Eq,
    I32Ne,
    I32LtS,
    I32LtU,
    I32GtS,
    I32GtU,
    I32LeS,
    I32LeU,
    I32GeS,
    I32GeU,
    I64Eqz,
    I64Eq,
    I64Ne,
    I64LtS,
    I64LtU,
    I64GtS,
    I64GtU,
    I64LeS,
    I64LeU,
    I64GeS,
    I64GeU,
    F32Eq,
    F32Ne,
    F32Lt,
    F32Gt,
    F32Le,
    F32Ge,
    F64Eq,
    F64Ne,
    F64Lt,
    F64Gt,
    F64Le,
    F64Ge,
    I32Clz,
    I32Ctz,
    I32Popcnt,
    I32Add,
    I32Sub,
    I32Mul,
    I32DivS,
    I32DivU,
    I32RemS,
    I32RemU,
    I32And,
    I32Or,
    I32Xor,
    I32Shl,
    I32ShrS,
    I32ShrU,
    I32Rotl,
    I32Rotr,
    I64Clz,
    I64Ctz,
    I64Popcnt,
    I64Add,
    I64Sub,
    I64Mul,
    I64DivS,
    I64DivU,
    I64RemS,
    I64RemU,
    I64And,
    I64Or,
    I64Xor,
    I64Shl,
    I64ShrS,
    I64ShrU,
    I64Rotl,
    I64Rotr,
    F32Abs,
    F32Neg,
    F32Ceil,
    F32Floor,
    F32Trunc,
    F32Nearest,
    F32Sqrt,
    F32Add,
    F32Sub,
    F32Mul,
    F32Div,
    F32Min,
    F32Max,
    F32Copysign,
    F64Abs,
    F64Neg,
    F64Ceil,
    F64Floor,
    F64Trunc,
    F64Nearest,
    F64Sqrt,
    F64Add,
    F64Sub,
    F64Mul,
    F64Div,
    F64Min,
    F64Max,
    F64Copysign,
    I32WrapI64,
    I32TruncSF32,
    I32TruncUF32,
    I32TruncSF64,
    I32TruncUF64,
    I64ExtendSI32,
    I64ExtendUI32,
    I64TruncSF32,
    I64TruncUF32,
    I64TruncSF64,
    I64TruncUF64,
    F32ConvertSI32,
    F32ConvertUI32,
    F32ConvertSI64,
    F32ConvertUI64,
    F32DemoteF64,
    F64ConvertSI32,
    F64ConvertUI32,
    F64ConvertSI64,
    F64ConvertUI64,
    F64PromoteF32,
    I32ReinterpretF32,
    I64ReinterpretF64,
    F32ReinterpretI32,
    F64ReinterpretI64,
    I32Extend8S,
    I32Extend16S,
    I64Extend8S,
    I64Extend16S,
    I64Extend32S,

    // 0xFC operators
    // Non-trapping Float-to-int Conversions
    I32TruncSSatF32,
    I32TruncUSatF32,
    I32TruncSSatF64,
    I32TruncUSatF64,
    I64TruncSSatF32,
    I64TruncUSatF32,
    I64TruncSSatF64,
    I64TruncUSatF64,

    // 0xFC operators
    // bulk memory https://github.com/WebAssembly/bulk-memory-operations/blob/master/proposals/bulk-memory-operations/Overview.md
    MemoryInit { segment: u32 },
    DataDrop { segment: u32 },
    MemoryCopy,
    MemoryFill,
    TableInit { segment: u32 },
    ElemDrop { segment: u32 },
    TableCopy,
    TableGet { table: u32 },
    TableSet { table: u32 },
    TableGrow { table: u32 },
    TableSize { table: u32 },

    // 0xFE operators
    // https://github.com/WebAssembly/threads/blob/master/proposals/threads/Overview.md
    Wake { memarg: MemoryImmediate },
    I32Wait { memarg: MemoryImmediate },
    I64Wait { memarg: MemoryImmediate },
    Fence { flags: u8 },
    I32AtomicLoad { memarg: MemoryImmediate },
    I64AtomicLoad { memarg: MemoryImmediate },
    I32AtomicLoad8U { memarg: MemoryImmediate },
    I32AtomicLoad16U { memarg: MemoryImmediate },
    I64AtomicLoad8U { memarg: MemoryImmediate },
    I64AtomicLoad16U { memarg: MemoryImmediate },
    I64AtomicLoad32U { memarg: MemoryImmediate },
    I32AtomicStore { memarg: MemoryImmediate },
    I64AtomicStore { memarg: MemoryImmediate },
    I32AtomicStore8 { memarg: MemoryImmediate },
    I32AtomicStore16 { memarg: MemoryImmediate },
    I64AtomicStore8 { memarg: MemoryImmediate },
    I64AtomicStore16 { memarg: MemoryImmediate },
    I64AtomicStore32 { memarg: MemoryImmediate },
    I32AtomicRmwAdd { memarg: MemoryImmediate },
    I64AtomicRmwAdd { memarg: MemoryImmediate },
    I32AtomicRmw8UAdd { memarg: MemoryImmediate },
    I32AtomicRmw16UAdd { memarg: MemoryImmediate },
    I64AtomicRmw8UAdd { memarg: MemoryImmediate },
    I64AtomicRmw16UAdd { memarg: MemoryImmediate },
    I64AtomicRmw32UAdd { memarg: MemoryImmediate },
    I32AtomicRmwSub { memarg: MemoryImmediate },
    I64AtomicRmwSub { memarg: MemoryImmediate },
    I32AtomicRmw8USub { memarg: MemoryImmediate },
    I32AtomicRmw16USub { memarg: MemoryImmediate },
    I64AtomicRmw8USub { memarg: MemoryImmediate },
    I64AtomicRmw16USub { memarg: MemoryImmediate },
    I64AtomicRmw32USub { memarg: MemoryImmediate },
    I32AtomicRmwAnd { memarg: MemoryImmediate },
    I64AtomicRmwAnd { memarg: MemoryImmediate },
    I32AtomicRmw8UAnd { memarg: MemoryImmediate },
    I32AtomicRmw16UAnd { memarg: MemoryImmediate },
    I64AtomicRmw8UAnd { memarg: MemoryImmediate },
    I64AtomicRmw16UAnd { memarg: MemoryImmediate },
    I64AtomicRmw32UAnd { memarg: MemoryImmediate },
    I32AtomicRmwOr { memarg: MemoryImmediate },
    I64AtomicRmwOr { memarg: MemoryImmediate },
    I32AtomicRmw8UOr { memarg: MemoryImmediate },
    I32AtomicRmw16UOr { memarg: MemoryImmediate },
    I64AtomicRmw8UOr { memarg: MemoryImmediate },
    I64AtomicRmw16UOr { memarg: MemoryImmediate },
    I64AtomicRmw32UOr { memarg: MemoryImmediate },
    I32AtomicRmwXor { memarg: MemoryImmediate },
    I64AtomicRmwXor { memarg: MemoryImmediate },
    I32AtomicRmw8UXor { memarg: MemoryImmediate },
    I32AtomicRmw16UXor { memarg: MemoryImmediate },
    I64AtomicRmw8UXor { memarg: MemoryImmediate },
    I64AtomicRmw16UXor { memarg: MemoryImmediate },
    I64AtomicRmw32UXor { memarg: MemoryImmediate },
    I32AtomicRmwXchg { memarg: MemoryImmediate },
    I64AtomicRmwXchg { memarg: MemoryImmediate },
    I32AtomicRmw8UXchg { memarg: MemoryImmediate },
    I32AtomicRmw16UXchg { memarg: MemoryImmediate },
    I64AtomicRmw8UXchg { memarg: MemoryImmediate },
    I64AtomicRmw16UXchg { memarg: MemoryImmediate },
    I64AtomicRmw32UXchg { memarg: MemoryImmediate },
    I32AtomicRmwCmpxchg { memarg: MemoryImmediate },
    I64AtomicRmwCmpxchg { memarg: MemoryImmediate },
    I32AtomicRmw8UCmpxchg { memarg: MemoryImmediate },
    I32AtomicRmw16UCmpxchg { memarg: MemoryImmediate },
    I64AtomicRmw8UCmpxchg { memarg: MemoryImmediate },
    I64AtomicRmw16UCmpxchg { memarg: MemoryImmediate },
    I64AtomicRmw32UCmpxchg { memarg: MemoryImmediate },

    // 0xFD operators
    // SIMD https://github.com/WebAssembly/simd/blob/master/proposals/simd/BinarySIMD.md
    V128Load { memarg: MemoryImmediate },
    V128Store { memarg: MemoryImmediate },
    V128Const { value: V128 },
    I8x16Splat,
    I8x16ExtractLaneS { lane: SIMDLaneIndex },
    I8x16ExtractLaneU { lane: SIMDLaneIndex },
    I8x16ReplaceLane { lane: SIMDLaneIndex },
    I16x8Splat,
    I16x8ExtractLaneS { lane: SIMDLaneIndex },
    I16x8ExtractLaneU { lane: SIMDLaneIndex },
    I16x8ReplaceLane { lane: SIMDLaneIndex },
    I32x4Splat,
    I32x4ExtractLane { lane: SIMDLaneIndex },
    I32x4ReplaceLane { lane: SIMDLaneIndex },
    I64x2Splat,
    I64x2ExtractLane { lane: SIMDLaneIndex },
    I64x2ReplaceLane { lane: SIMDLaneIndex },
    F32x4Splat,
    F32x4ExtractLane { lane: SIMDLaneIndex },
    F32x4ReplaceLane { lane: SIMDLaneIndex },
    F64x2Splat,
    F64x2ExtractLane { lane: SIMDLaneIndex },
    F64x2ReplaceLane { lane: SIMDLaneIndex },
    I8x16Eq,
    I8x16Ne,
    I8x16LtS,
    I8x16LtU,
    I8x16GtS,
    I8x16GtU,
    I8x16LeS,
    I8x16LeU,
    I8x16GeS,
    I8x16GeU,
    I16x8Eq,
    I16x8Ne,
    I16x8LtS,
    I16x8LtU,
    I16x8GtS,
    I16x8GtU,
    I16x8LeS,
    I16x8LeU,
    I16x8GeS,
    I16x8GeU,
    I32x4Eq,
    I32x4Ne,
    I32x4LtS,
    I32x4LtU,
    I32x4GtS,
    I32x4GtU,
    I32x4LeS,
    I32x4LeU,
    I32x4GeS,
    I32x4GeU,
    F32x4Eq,
    F32x4Ne,
    F32x4Lt,
    F32x4Gt,
    F32x4Le,
    F32x4Ge,
    F64x2Eq,
    F64x2Ne,
    F64x2Lt,
    F64x2Gt,
    F64x2Le,
    F64x2Ge,
    V128Not,
    V128And,
    V128Or,
    V128Xor,
    V128Bitselect,
    I8x16Neg,
    I8x16AnyTrue,
    I8x16AllTrue,
    I8x16Shl,
    I8x16ShrS,
    I8x16ShrU,
    I8x16Add,
    I8x16AddSaturateS,
    I8x16AddSaturateU,
    I8x16Sub,
    I8x16SubSaturateS,
    I8x16SubSaturateU,
    I8x16Mul,
    I16x8Neg,
    I16x8AnyTrue,
    I16x8AllTrue,
    I16x8Shl,
    I16x8ShrS,
    I16x8ShrU,
    I16x8Add,
    I16x8AddSaturateS,
    I16x8AddSaturateU,
    I16x8Sub,
    I16x8SubSaturateS,
    I16x8SubSaturateU,
    I16x8Mul,
    I32x4Neg,
    I32x4AnyTrue,
    I32x4AllTrue,
    I32x4Shl,
    I32x4ShrS,
    I32x4ShrU,
    I32x4Add,
    I32x4Sub,
    I32x4Mul,
    I64x2Neg,
    I64x2AnyTrue,
    I64x2AllTrue,
    I64x2Shl,
    I64x2ShrS,
    I64x2ShrU,
    I64x2Add,
    I64x2Sub,
    F32x4Abs,
    F32x4Neg,
    F32x4Sqrt,
    F32x4Add,
    F32x4Sub,
    F32x4Mul,
    F32x4Div,
    F32x4Min,
    F32x4Max,
    F64x2Abs,
    F64x2Neg,
    F64x2Sqrt,
    F64x2Add,
    F64x2Sub,
    F64x2Mul,
    F64x2Div,
    F64x2Min,
    F64x2Max,
    I32x4TruncSF32x4Sat,
    I32x4TruncUF32x4Sat,
    I64x2TruncSF64x2Sat,
    I64x2TruncUF64x2Sat,
    F32x4ConvertSI32x4,
    F32x4ConvertUI32x4,
    F64x2ConvertSI64x2,
    F64x2ConvertUI64x2,
    V8x16Swizzle,
    V8x16Shuffle { lanes: [SIMDLaneIndex; 16] },
    I8x16LoadSplat { memarg: MemoryImmediate },
    I16x8LoadSplat { memarg: MemoryImmediate },
    I32x4LoadSplat { memarg: MemoryImmediate },
    I64x2LoadSplat { memarg: MemoryImmediate },
}
