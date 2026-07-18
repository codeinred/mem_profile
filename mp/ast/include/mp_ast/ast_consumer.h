#pragma once

#include <mp_ast/ast_env.h>
#include <mp_ast/dtor_visitor.h>

namespace mp {
/// Instruments destructors in two passes:
///
/// 1. An eager pass from HandleTopLevelDecl. CodeGen emits external-linkage
///    definitions from its own HandleTopLevelDecl, mid-parse; rewriting here
///    beats it to every decl because the plugin is an AddBeforeMainAction, so
///    the MultiplexConsumer runs our callbacks before CodeGen's. Sema also
///    delivers template instantiations through this callback.
/// 2. A sweep pass from HandleTranslationUnit. Catches everything Sema
///    defines lazily and never re-delivers: implicit destructors, defaulted
///    destructors defined on first use, and any straggling instantiations.
///    These are all emitted with deferred linkage (linkonce_odr/weak_odr), so
///    CodeGen has not emitted them yet — its own HandleTranslationUnit, where
///    deferred decls are flushed, runs after this one.
///
/// The eager pass is *required* only for external-linkage (out-of-line,
/// user-provided) destructor definitions; the sweep is sufficient for
/// everything with deferred linkage by construction.
class ast_consumer : public ASTConsumer {
    /// Persists across callbacks so the `rewritten` bookkeeping spans the
    /// whole TU. Safe to construct in the consumer's constructor: the
    /// ASTContext is created before CreateASTConsumer is invoked.
    dtor_visitor visitor;

  public:
    ast_consumer(CompilerInstance& compiler) : visitor(compiler) {
        bool print_all = get_env_flag("MEM_PROFILE_PRINT_ALL");

        visitor.print_dtor_ast  = print_all || get_env_flag("MEM_PROFILE_PRINT_AST");
        visitor.print_dtor_body = print_all || get_env_flag("MEM_PROFILE_PRINT_BODY");
        // Print the name of the dtor if anything is printed
        visitor.print_dtor_name = print_all                //
                               || visitor.print_dtor_ast  //
                               || visitor.print_dtor_body //
                               || get_env_flag("MEM_PROFILE_PRINT_NAME");
    }

    bool HandleTopLevelDecl(DeclGroupRef group) override {
        for (Decl* decl : group) {
            visitor.TraverseDecl(decl);
        }
        return true;
    }

    void HandleTranslationUnit(ASTContext& ctx) override {
        visitor.allow_implicit = true;
        visitor.TraverseDecl(ctx.getTranslationUnitDecl());
    }
};
} // namespace mp
