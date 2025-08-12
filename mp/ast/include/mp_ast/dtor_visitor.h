#pragma once

#include <clang/AST/AST.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/Expr.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Type.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendPluginRegistry.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Sema/Sema.h>
#include <llvm/Support/raw_ostream.h>

#include <mp_ast/ast_tools.h>
#include <mp_error/error.h>
#include <unordered_set>

namespace mp {
using namespace clang;

struct dtor_visitor : public RecursiveASTVisitor<dtor_visitor>, ast_tools {
  public:
    std::unordered_set<CXXDestructorDecl*> dtors;

    dtor_visitor(CompilerInstance& CI) : ast_tools(CI) {}

    bool VisitCXXRecordDecl(CXXRecordDecl* R) { return true; }

    // bool VisitCXXDestructorDecl(CXXDestructorDecl* D) {
    //     // Skip deleted destructors
    //     if (D->isDeleted()) {
    //         return true;
    //     }

    //     // Check if destructor is non-trivial
    //     if (D->getParent()->hasTrivialDestructor()) {
    //         return true;
    //     }

    //     if (D->isCanonicalDecl()) {
    //         dtors.insert(D);
    //     }

    //     return true;
    // }

    bool VisitCXXDestructorDecl(CXXDestructorDecl* dtor) {
        dtors.insert(dtor);
        return true;
    }

    void maybe_perform_rewrite(CXXDestructorDecl* dtor) {
        // Skip deleted destructors
        if (dtor->isDeleted()) {
            return;
        }

        auto parent = dtor->getParent();
        if (parent == nullptr) {
            llvm::outs() << "Couldn't get parent for dtor ";
            dtor->getNameForDiagnostic(llvm::outs(), ctx.getPrintingPolicy(), true);
            llvm::outs() << '\n';
            return;
        }

        if (!parent->hasDefinition()) {
            return;
        }
        // Check if destructor is non-trivial
        if (parent->hasTrivialDestructor()) {
            return;
        }

        /// Skip destructors that aren't actually instantiated
        if (dtor->isTemplated() && !dtor->isTemplateInstantiation()) {
            return;
        }

        bool has_body    = dtor->doesThisDeclarationHaveABody();
        bool is_implicit = dtor->isImplicit();

        if (has_body || is_implicit) {
            rewrite_dtor(dtor);
        }
    }

    void rewrite_dtor(CXXDestructorDecl* dtor) {

        // Get the class type
        CXXRecordDecl* class_decl = dtor->getParent();
        QualType       class_type = ctx.getTypeDeclType(class_decl);

        FunctionDecl* save_state_decl = get_hook();

        if (save_state_decl == nullptr) {
            /// TODO: add proper error handling
            llvm::outs() << "Unable to find `save_state`\n";
            return;
        }

        if (dtor->isDefaulted() || dtor->isExplicitlyDefaulted()) {
            dtor->setDefaulted(false);
            dtor->setExplicitlyDefaulted(false);
        }
        dtor->addAttr(NoInlineAttr::Create(ctx));

        auto decl_context = dtor->getDeclContext();
        if (!dtor->hasBody()) {
            auto loc  = dtor->getSourceRange().getBegin();
            auto hook = invoke_hook(loc, class_type, save_state_decl, decl_context);
            dtor->setBody(compound_stmt(loc, hook));
        } else {
            auto OldBody   = dtor->getBody();
            auto loc_begin = OldBody->getBeginLoc();
            auto hook      = invoke_hook(loc_begin, class_type, save_state_decl, decl_context);

            if (CompoundStmt* CS = dyn_cast<CompoundStmt>(OldBody)) {
                dtor->setBody(prepend(CS, hook));
            } else {
                // Body is a single statement, wrap in compound statement with the hook prepended
                dtor->setBody(join(hook, OldBody));
            }
        }
        auto& outs = llvm::outs();
        outs << "Rewrote CXX Destructor '";
        auto&& sm  = ctx.getSourceManager();
        auto   pol = PrintingPolicy(LangOptions());
        dtor->getNameForDiagnostic(outs, pol, true);

        outs << " @ ";
        dtor->getLocation().print(outs, sm);
        outs << '\n';
        dtor->print(outs, 0, false);
    }

    void rewrite_dtors() {
        for (auto dtor : dtors) {
            maybe_perform_rewrite(dtor);
        }
    }

    /// We want to visit implicitly generated destructors in order to instrument
    /// them
    bool shouldVisitImplicitCode() const { return true; }

    /// We want to visit template instantiations, since we need to insert
    /// the hook into these
    bool shouldVisitTemplateInstantiations() const { return true; }
};
} // namespace mp
