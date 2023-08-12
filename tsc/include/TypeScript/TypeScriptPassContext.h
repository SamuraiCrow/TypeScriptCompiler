#include "TypeScript/TypeScriptDialect.h"
#include "TypeScript/TypeScriptOps.h"
#include "TypeScript/DataStructs.h"

#include "mlir/IR/PatternMatch.h"

using namespace mlir;
using namespace ::typescript;
namespace mlir_ts = mlir::typescript;

namespace
{

struct TSContext
{
    TSContext() = default;

    // options
    CompileOptions compileOptions;
    bool isJit;

    // name, break, continue
    mlir::DenseMap<Operation *, mlir::Block *> jumps;
    mlir::DenseMap<Operation *, mlir::Value> catchOpData;
    mlir::DenseMap<Operation *, mlir::Block *> unwind;
    mlir::DenseMap<Operation *, Operation *> parentTryOp;
    mlir::DenseMap<Operation *, mlir::Block *> landingBlockOf;
    mlir::Block *returnBlock;
};

struct TSFunctionContext
{
    TSFunctionContext() = default;

    mlir::SmallVector<mlir::Block *> stateLabels;
};

template <typename OpTy> class TsPattern : public OpRewritePattern<OpTy>
{
  public:
    TsPattern<OpTy>(MLIRContext *context, TSContext *tsContext, TSFunctionContext *tsFuncContext, PatternBenefit benefit = 1)
        : OpRewritePattern<OpTy>::OpRewritePattern(context, benefit), tsContext(tsContext), tsFuncContext(tsFuncContext)
    {
    }

  protected:
    TSContext *tsContext;
    TSFunctionContext *tsFuncContext;
};

} // namespace