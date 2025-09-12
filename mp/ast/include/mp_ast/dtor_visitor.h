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

#include <mp_core/colors.h>

#include <mp_ast/ast_tools.h>
#include <mp_error/error.h>
#include <unordered_set>

namespace mp {
using namespace clang;

struct dtor_visitor : public RecursiveASTVisitor<dtor_visitor>, ast_tools {
  public:
    std::unordered_set<CXXDestructorDecl*> dtors;

    /// If true, print the names of dtors as they're rewritten
    bool print_dtor_name = false;
    /// If true, print the body of any dtors that are rewritten by the program
    bool print_dtor_body = false;
    /// If true, print the abstract syntax tree of the destructor
    bool print_dtor_ast  = false;

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
            dtor->getNameForDiagnostic(llvm::outs(), pol, true);
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

        // Get the class corresponding to this dtor, and the type of that class
        CXXRecordDecl* record = dtor->getParent();
        QualType       type   = ctx.getTypeDeclType(record);

        // Find the hook called by the payload
        FunctionDecl* hook = find_function_decl("save_state");

        if (hook == nullptr) {
            /// TODO: add proper error handling
            llvm::outs() << "Unable to find `save_state`\n";
            return;
        }

        // Mark the dtor as not defaulted, since we're adding code for it
        if (dtor->isDefaulted() || dtor->isExplicitlyDefaulted()) {
            dtor->setDefaulted(false);
            dtor->setExplicitlyDefaulted(false);
        }

        auto dtor_start = dtor->getBeginLoc();
        auto body_start = dtor->getBodyRBrace();


        // Inject the payload into the dtor ast. This payload includes the
        // call to the hook, as well as the creation of some static variables
        // which provide information about the class.
        auto payload = invoke_hook(body_start, type, hook, record, dtor);

        if (!dtor->hasBody()) {
            dtor->setBody(compound_stmt(body_start, payload));
        } else {
            auto OldBody = dtor->getBody();
            if (CompoundStmt* CS = dyn_cast<CompoundStmt>(OldBody)) {
                dtor->setBody(prepend(CS, payload));
            } else {
                // Body is a single statement, wrap in compound statement with the hook prepended
                dtor->setBody(join(payload, OldBody));
            }
        }

        // Mark the destructor as 'noinline', to ensure it isn't inlined
        dtor->addAttr(NoInlineAttr::Create(ctx, SourceRange(dtor_start)));

        bool print_any = print_dtor_ast || print_dtor_body || print_dtor_name;

        if (print_any) {
            auto&  outs = llvm::outs();
            auto&& sm   = ctx.getSourceManager();
            if (print_dtor_name) {
                outs << mp::colors::BW;
                outs << "Rewrote ";
                outs << mp::colors::BG;
                dtor->getNameForDiagnostic(outs, pol, true);
                outs << mp::colors::Re << " @ " << mp::colors::BC;
                dtor->getLocation().print(outs, sm);
                outs << mp::colors::Re << '\n';
            }
            if (print_dtor_body) {

                dtor->print(outs, pol, 0, false);
            }
            if (print_dtor_ast) {
                dtor->dumpColor();
            }
        }
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
