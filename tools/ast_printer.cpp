#include <mp_ast/ast_fmt.h>
#include <mp_ast/ast_printer.h>
#include <mp_error/error.h>
#include <mp_fs/fs.h>

#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <filesystem>
#include <fmt/core.h>
#include <llvm/Support/CommandLine.h>
#include <string>

namespace {
// These options register themselves with llvm
auto help      = llvm::cl::extrahelp(clang::tooling::CommonOptionsParser::HelpMessage);
auto more_help = llvm::cl::extrahelp(""); // more help is blank for now
} // namespace

namespace fs = std::filesystem;


namespace mp {
using namespace clang;

#define INFO(x) fmt::println("{}: {}", #x, x)
struct printer_visitor : public RecursiveASTVisitor<printer_visitor> {
    ASTContext& ctx;

    printer_visitor(ASTContext& ctx) : ctx(ctx) { here(); }

    bool VisitDeclRefExpr(DeclRefExpr* decl) {
        decl->dumpColor();
        INFO(decl->refersToEnclosingVariableOrCapture());
        return true;
    }
    bool VisitVarDecl(VarDecl* decl) {

        fmt::println("Variable decl @ {}", ctx.getFullLoc(decl->getLocation()));
        fmt::println("Storage class: {}", decl->getStorageClass());
        auto ctx = decl->getDeclContext();

        fmt::println("Decl kind: {}", ctx->getDeclKindName());

        return true;
    }

    bool VisitImplicitCastExpr(ImplicitCastExpr* expr) {
        fmt::println("ExprValueKind: {}", expr->getValueKind());
        return true;
    }
};

class ast_printer_consumer : public ASTConsumer {
  public:
    CompilerInstance& compiler;

    ast_printer_consumer(CompilerInstance& compiler) : compiler(compiler) {}

    void HandleTranslationUnit(ASTContext& ctx) override {
        here("Handling Translation Unit");
        printer_visitor visitor(ctx);
        visitor.TraverseDecl(ctx.getTranslationUnitDecl());
    }
};
class PrintAstAction : public ASTFrontendAction {
  protected:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& compiler, StringRef) override {
        here();
        return std::make_unique<ast_printer_consumer>(compiler);
    }


    bool usesPreprocessorOnly() const override { return false; }
};

} // namespace mp

int main(int argc, char const* argv[]) try {


    for (int i = 1; i < argc; i++) {
        fs::path p(argv[i]);

        auto contents = mp::read_file(p);

        clang::tooling::runToolOnCode(std::make_unique<mp::PrintAstAction>(), contents, p.string());
        //clang::tooling::runToolOnCode(std::make_unique<clang::SyntaxOnlyAction>(), contents, p.string());
    }
    return 0;
    using namespace clang::tooling;

    using namespace llvm;

    auto category = cl::OptionCategory("AST Display Tool");

    auto try_parse = CommonOptionsParser::create(argc, argv, category);

    if (!try_parse) {
        llvm::errs() << try_parse.takeError() << '\n';
        return 1;
    }

    auto& parser = try_parse.get();

    auto tool = ClangTool(parser.getCompilations(), parser.getSourcePathList());

    auto action = newFrontendActionFactory<clang::SyntaxOnlyAction>();
    return tool.run(action.get());
} catch (mp::mp_error const& err) {
    mp::terminate_with_error(err);
} catch (std::exception const& err) {
    mp::terminate_with_error(err);
}
