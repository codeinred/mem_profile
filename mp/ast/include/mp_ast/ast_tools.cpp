#include <mp_ast/ast_tools.h>

namespace mp {

clang::CompoundStmt* ast_tools::prepend(CompoundStmt* body, Stmt* stmt) {
    return prepend(body, view<Stmt*>(&stmt, 1));
}

clang::CompoundStmt* ast_tools::prepend(CompoundStmt* body, view<Stmt*> stmt) {
    auto body_size  = body->size();
    auto new_items  = stmt.size();
    auto total_size = body_size + new_items;
    auto dest       = ctx.Allocate<Stmt*>(total_size);

    auto elems = body->body();
    std::copy_n(stmt.data(), new_items, dest);
    std::copy_n(elems.begin(), body_size, dest + new_items);

    auto source_range = body->getSourceRange();
    return CompoundStmt::Create(ctx,
                                ArrayRef(dest, total_size),
                                body->getStoredFPFeaturesOrDefault(),
                                source_range.getBegin(),
                                source_range.getEnd());
}

clang::CompoundStmt* ast_tools::join(view<Stmt*> first, Stmt* last) {
    auto dest = ctx.Allocate<Stmt*>(first.size() + 1);
    std::copy_n(first.data(), first.size(), dest);
    dest[first.size()] = last;
    return CompoundStmt::Create(ctx,
                                ArrayRef(dest, dest + first.size() + 1),
                                FPOptionsOverride(),
                                dest[0]->getBeginLoc(),
                                last->getEndLoc());
}
CompoundStmt* ast_tools::join(Stmt* first, Stmt* last) {
    return join(view<Stmt*>(&first, 1), last);
}
} // namespace mp
