#ifndef MLIR_TYPESCRIPT_LOWERTOLLVMLOGIC_H_
#define MLIR_TYPESCRIPT_LOWERTOLLVMLOGIC_H_

#include "TypeScript/Config.h"
#include "TypeScript/Defines.h"
#include "TypeScript/Passes.h"
#include "TypeScript/TypeScriptDialect.h"
#include "TypeScript/TypeScriptOps.h"

#include "TypeScript/LowerToLLVM/TypeHelper.h"
#include "TypeScript/LowerToLLVM/TypeConverterHelper.h"
#include "TypeScript/LowerToLLVM/LLVMTypeConverterHelper.h"
#include "TypeScript/LowerToLLVM/CodeLogicHelper.h"
#include "TypeScript/LowerToLLVM/LLVMCodeHelper.h"
#include "TypeScript/LowerToLLVM/LLVMRTTIHelperVCWin32.h"
#include "TypeScript/LowerToLLVM/AssertLogic.h"
#include "TypeScript/LowerToLLVM/ConvertLogic.h"
#include "TypeScript/LowerToLLVM/CastLogicHelper.h"
#include "TypeScript/LowerToLLVM/OptionalLogicHelper.h"
#include "TypeScript/LowerToLLVM/TypeOfOpHelper.h"

#include "TypeScript/CommonGenLogic.h"

#include "scanner_enums.h"

using namespace mlir;
namespace mlir_ts = mlir::typescript;

namespace typescript
{

template <typename T> Value LLVMCodeHelper::_MemoryAlloc(mlir::Value sizeOfAlloc, MemoryAllocSet zero)
{
    TypeHelper th(rewriter);
    TypeConverterHelper tch(typeConverter);
    CodeLogicHelper clh(op, rewriter);

    auto loc = op->getLoc();

    auto i8PtrTy = th.getI8PtrType();
    auto mallocFuncOp = getOrInsertFunction("malloc", th.getFunctionType(i8PtrTy, {th.getIndexType()}));

    auto effectiveSize = sizeOfAlloc;
    if (effectiveSize.getType() != th.getIndexType())
    {
        CastLogicHelper castLogic(op, rewriter, tch);
        effectiveSize = castLogic.cast(effectiveSize, th.getIndexType());
    }

    auto callResults = rewriter.create<LLVM::CallOp>(loc, mallocFuncOp, ValueRange{effectiveSize});
    auto ptr = callResults.getResult(0);

    if (zero == MemoryAllocSet::Zero)
    {
        auto memsetFuncOp = getOrInsertFunction("memset", th.getFunctionType(i8PtrTy, {i8PtrTy, th.getI32Type(), th.getIndexType()}));
        auto const0 = clh.createI32ConstantOf(0);
        rewriter.create<LLVM::CallOp>(loc, memsetFuncOp, ValueRange{ptr, const0, effectiveSize});
    }

    return ptr;
}

template <typename T> Value LLVMCodeHelper::_MemoryRealloc(mlir::Value ptrValue, mlir::Value sizeOfAlloc)
{
    TypeHelper th(rewriter);
    TypeConverterHelper tch(typeConverter);

    auto loc = op->getLoc();

    auto i8PtrTy = th.getI8PtrType();

    auto effectivePtrValue = ptrValue;
    if (ptrValue.getType() != i8PtrTy)
    {
        effectivePtrValue = rewriter.create<LLVM::BitcastOp>(loc, i8PtrTy, ptrValue);
    }

    auto mallocFuncOp = getOrInsertFunction("realloc", th.getFunctionType(i8PtrTy, {i8PtrTy, th.getIndexType()}));

    auto effectiveSize = sizeOfAlloc;
    if (effectiveSize.getType() != th.getIndexType())
    {
        CastLogicHelper castLogic(op, rewriter, tch);
        effectiveSize = castLogic.cast(effectiveSize, th.getIndexType());
    }

    auto callResults = rewriter.create<LLVM::CallOp>(loc, mallocFuncOp, ValueRange{effectivePtrValue, effectiveSize});
    return callResults.getResult(0);
}

template <typename T> mlir::LogicalResult LLVMCodeHelper::_MemoryFree(mlir::Value ptrValue)
{
    TypeHelper th(rewriter);
    TypeConverterHelper tch(typeConverter);

    auto loc = op->getLoc();

    auto i8PtrTy = th.getI8PtrType();

    auto freeFuncOp = getOrInsertFunction("free", th.getFunctionType(th.getVoidType(), {i8PtrTy}));

    auto casted = rewriter.create<LLVM::BitcastOp>(loc, i8PtrTy, ptrValue);

    rewriter.create<LLVM::CallOp>(loc, freeFuncOp, ValueRange{casted});

    return mlir::success();
}

template <typename StdIOpTy, typename V1, V1 v1, typename StdFOpTy, typename V2, V2 v2>
mlir::Value LogicOp_(Operation *, SyntaxKind, mlir::Value, mlir::Value, PatternRewriter &, LLVMTypeConverter &);

template <typename UnaryOpTy, typename StdIOpTy, typename StdFOpTy> void UnaryOp(UnaryOpTy &unaryOp, PatternRewriter &builder)
{
    auto oper = unaryOp.operand1();
    auto type = oper.getType();
    if (type.isIntOrIndex())
    {
        builder.replaceOpWithNewOp<StdIOpTy>(unaryOp, type, oper);
    }
    else if (!type.isIntOrIndex() && type.isIntOrIndexOrFloat())
    {
        builder.replaceOpWithNewOp<StdFOpTy>(unaryOp, type, oper);
    }
    else
    {
        emitError(unaryOp.getLoc(), "Not implemented operator for type 1: '") << type << "'";
        llvm_unreachable("not implemented");
    }
}

template <typename BinOpTy, typename StdIOpTy, typename StdFOpTy> void BinOp(BinOpTy &binOp, PatternRewriter &builder)
{
    auto loc = binOp->getLoc();

    auto left = binOp->getOperand(0);
    auto right = binOp->getOperand(1);
    auto leftType = left.getType();
    if (leftType.isIntOrIndex())
    {
        builder.replaceOpWithNewOp<StdIOpTy>(binOp, left, right);
    }
    else if (!leftType.isIntOrIndex() && leftType.isIntOrIndexOrFloat())
    {
        builder.replaceOpWithNewOp<StdFOpTy>(binOp, left, right);
    }
    else if (leftType.template dyn_cast_or_null<mlir_ts::NumberType>())
    {
        auto castLeft = builder.create<mlir_ts::CastOp>(loc, builder.getF32Type(), left);
        auto castRight = builder.create<mlir_ts::CastOp>(loc, builder.getF32Type(), right);
        builder.replaceOpWithNewOp<StdFOpTy>(binOp, castLeft, castRight);
    }
    else
    {
        emitError(binOp.getLoc(), "Not implemented operator for type 1: '") << leftType << "'";
        llvm_unreachable("not implemented");
    }
}

template <typename StdIOpTy, typename V1, V1 v1, typename StdFOpTy, typename V2, V2 v2>
mlir::Value LogicOp_(Operation *binOp, SyntaxKind op, mlir::Value left, mlir::Value right, PatternRewriter &builder,
                     LLVMTypeConverter &typeConverter)
{
    auto loc = binOp->getLoc();

    LLVMTypeConverterHelper llvmtch(typeConverter);

    auto leftType = left.getType();
    auto rightType = right.getType();

    if (leftType.isa<mlir_ts::OptionalType>() || rightType.isa<mlir_ts::OptionalType>())
    {
        OptionalLogicHelper olh(binOp, builder, typeConverter);
        auto value = olh.logicalOp<StdIOpTy, V1, v1, StdFOpTy, V2, v2>(binOp, op);
        return value;
    }
    else if (leftType.isIntOrIndex() || leftType.dyn_cast_or_null<mlir_ts::BooleanType>())
    {
        auto value = builder.create<StdIOpTy>(loc, v1, left, right);
        return value;
    }
    else if (!leftType.isIntOrIndex() && leftType.isIntOrIndexOrFloat())
    {
        auto value = builder.create<StdFOpTy>(loc, v2, left, right);
        return value;
    }
    else if (leftType.dyn_cast_or_null<mlir_ts::NumberType>())
    {
        auto castLeft = builder.create<mlir_ts::CastOp>(loc, builder.getF32Type(), left);
        auto castRight = builder.create<mlir_ts::CastOp>(loc, builder.getF32Type(), right);
        auto value = builder.create<StdFOpTy>(loc, v2, castLeft, castRight);
        return value;
    }
    /*
    else if (auto leftEnumType = leftType.dyn_cast_or_null<mlir_ts::EnumType>())
    {
        auto castLeft = builder.create<mlir_ts::CastOp>(loc, leftEnumType.getElementType(), left);
        auto castRight = builder.create<mlir_ts::CastOp>(loc, leftEnumType.getElementType(), right);
        auto res = builder.create<StdFOpTy>(loc, v2, castLeft, castRight);
        builder.create<mlir_ts::CastOp>(binOp, leftEnumType, res);
        return value;
    }
    */
    else if (leftType.dyn_cast_or_null<mlir_ts::StringType>())
    {
        if (left.getType() != right.getType())
        {
            right = builder.create<mlir_ts::CastOp>(loc, left.getType(), right);
        }

        auto value = builder.create<mlir_ts::StringCompareOp>(loc, mlir_ts::BooleanType::get(builder.getContext()), left, right,
                                                              builder.getI32IntegerAttr((int)op));

        return value;
    }
    else if (leftType.dyn_cast_or_null<mlir_ts::AnyType>() || leftType.dyn_cast_or_null<mlir_ts::ClassType>())
    {
        // excluded string
        auto intPtrType = llvmtch.getIntPtrType(0);

        Value leftPtrValue = builder.create<LLVM::PtrToIntOp>(loc, intPtrType, left);
        Value rightPtrValue = builder.create<LLVM::PtrToIntOp>(loc, intPtrType, right);

        auto value = builder.create<StdIOpTy>(loc, v1, leftPtrValue, rightPtrValue);
        return value;
    }
    else
    {
        emitError(loc, "Not implemented operator for type 1: '") << leftType << "'";
        llvm_unreachable("not implemented");
    }
}

template <typename StdIOpTy, typename V1, V1 v1, typename StdFOpTy, typename V2, V2 v2>
mlir::Value LogicOp(Operation *binOp, SyntaxKind op, PatternRewriter &builder, LLVMTypeConverter &typeConverter)
{
    auto left = binOp->getOperand(0);
    auto right = binOp->getOperand(1);
    return LogicOp_<StdIOpTy, V1, v1, StdFOpTy, V2, v2>(binOp, op, left, right, builder, typeConverter);
}
} // namespace typescript

#endif // MLIR_TYPESCRIPT_LOWERTOLLVMLOGIC_H_
