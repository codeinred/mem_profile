#pragma once

#include <mp_ast/ast_env.h>
#include <mp_ast/dtor_visitor.h>

namespace mp {
class ast_consumer : public ASTConsumer {
    CompilerInstance& compiler;

  public:
    ast_consumer(CompilerInstance& compiler) : compiler(compiler) {}

    void HandleTranslationUnit(ASTContext& ctx) override {
        dtor_visitor v(compiler);

        bool print_all = get_env_flag("MEM_PROFILE_PRINT_ALL");

        v.print_dtor_ast  = print_all || get_env_flag("MEM_PROFILE_PRINT_AST");
        v.print_dtor_body = print_all || get_env_flag("MEM_PROFILE_PRINT_BODY");
        // Print the name of the dtor if anything is printed
        v.print_dtor_name = print_all         //
                         || v.print_dtor_ast  //
                         || v.print_dtor_body //
                         || get_env_flag("MEM_PROFILE_PRINT_NAME");

        v.TraverseDecl(ctx.getTranslationUnitDecl());
        v.rewrite_dtors();
    }
};
} // namespace mp
