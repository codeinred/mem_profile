#pragma once

#include <mp_ast/dtor_visitor.h>

namespace mp {
class ast_consumer : public ASTConsumer {
    CompilerInstance& compiler;

  public:
    ast_consumer(CompilerInstance& compiler) : compiler(compiler) {}

    void HandleTranslationUnit(ASTContext& ctx) override {
        dtor_visitor v(compiler);

        v.TraverseDecl(ctx.getTranslationUnitDecl());
        v.rewrite_dtors();
    }
};
} // namespace mp
