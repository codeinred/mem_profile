#pragma once

#include <mp_ast/ast_consumer.h>

#include <clang/Frontend/FrontendPluginRegistry.h>
namespace mp {
using namespace clang;

class mp_plugin_action : public PluginASTAction {
  protected:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& CI,
                                                   llvm::StringRef) override {
        return std::make_unique<ast_consumer>(CI);
    }

    bool ParseArgs(const CompilerInstance&         CI,
                   const std::vector<std::string>& args) override {
        DiagnosticsEngine& D = CI.getDiagnostics();
        for (auto const& arg : args) {
            unsigned DiagID =
                D.getCustomDiagID(DiagnosticsEngine::Error,
                                  "invalid argument '%0' - this plugin "
                                  "does not take any arguments");
            D.Report(DiagID) << arg;
        }

        return args.empty();
    }

    ActionType getActionType() override {
        return ActionType::AddBeforeMainAction;
    }

    void PrintHelp(llvm::raw_ostream& ros) {}
};
} // namespace mp
