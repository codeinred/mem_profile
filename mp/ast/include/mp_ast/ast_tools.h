#pragma once

#include <clang/AST/AST.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/Expr.h>
#include <clang/AST/RecordLayout.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Type.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendPluginRegistry.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Sema/Sema.h>
#include <llvm/Support/raw_ostream.h>
#include <span>

namespace mp {
using Loc = clang::SourceLocation;
using namespace clang;
template <class T> using view = std::span<T const>;

struct On {
    ASTContext& ctx;
    QualType    type;

    ImplicitCastExpr* decay_to_function_ptr(Expr* expr) {
        return ImplicitCastExpr::Create(ctx,
                                        ctx.getPointerType(type),
                                        CK_FunctionToPointerDecay,
                                        expr,
                                        nullptr,
                                        VK_PRValue,
                                        FPOptionsOverride());
    }
};
struct ast_tools {
    CompilerInstance& compiler;
    ASTContext&       ctx = compiler.getASTContext();
    PrintingPolicy    pol = ctx.getPrintingPolicy();

    ast_tools(CompilerInstance& compiler) : compiler(compiler) {
        // Ensures that tag keywords such as 'struct' or 'class' are suppressed
        // when printing the type name
        pol.adjustForCPlusPlus();
    }

    /// Prepends the given statement to a CompoundStatement, returning
    /// a new CompoundStatement
    CompoundStmt* prepend(CompoundStmt* body, Stmt* stmt);

    CompoundStmt* prepend(CompoundStmt* body, view<Stmt*> stmt);

    CompoundStmt* join(view<Stmt*> first, Stmt* last);
    CompoundStmt* join(Stmt* first, Stmt* last);

    CompoundStmt* compound_stmt(Loc loc, view<Stmt*> stmt) {
        return CompoundStmt::Create(ctx,
                                    array_ref(view(stmt.data(), stmt.size())),
                                    FPOptionsOverride(),
                                    loc,
                                    loc);
    }
    CompoundStmt* compound_stmt(Loc loc, Stmt* stmt) {
        return compound_stmt(loc, view<Stmt*>(&stmt, 1));
    }


    template <class T> ArrayRef<T> array_ref(std::span<T const> elems) {
        size_t count    = elems.size();
        T*     dest     = ctx.Allocate<T>(elems.size());
        T*     refBegin = dest;
        T*     refEnd   = dest + count;

        for (auto const& value : elems) {
            *dest = value;
            ++dest;
        }
        return ArrayRef<T>(refBegin, refEnd);
    }
    template <class T> ArrayRef<T> array_ref(std::initializer_list<T> elems) {

        return array_ref(std::span<T const>(elems.begin(), elems.size()));
    }

    template <class T> MutableArrayRef<T> mut_array_ref(std::initializer_list<T> elems) {
        size_t count    = elems.size();
        T*     dest     = ctx.Allocate<T>(elems.size());
        T*     refBegin = dest;
        T*     refEnd   = dest + count;

        for (auto const& value : elems) {
            *dest = value;
            ++dest;
        }
        return MutableArrayRef<T>(refBegin, refEnd);
    }

    template <class T> T* get_decl_by_type(StringRef name_str) {
        DeclarationName name      = &ctx.Idents.get(name_str);
        DeclContext*    TUContext = ctx.getTranslationUnitDecl();

        auto LookupResult = TUContext->lookup(name);
        for (auto* Decl : LookupResult) {
            if (auto* FD = dyn_cast<T>(Decl)) {
                return FD;
            }
        }
        return nullptr;
    }

    FunctionDecl* find_function_decl(StringRef name) {
        return get_decl_by_type<FunctionDecl>(name);
    }

    CXXRecordDecl* find_record_decl(StringRef name) {
        return get_decl_by_type<CXXRecordDecl>(name);
    }

    /// Returns an implicit cast to a void pointer for the given expression
    ImplicitCastExpr* to_void_ptr(Expr* expr) {
        return ImplicitCastExpr::Create(ctx,
                                        ctx.VoidPtrTy,
                                        CK_BitCast,
                                        expr,
                                        nullptr,
                                        VK_PRValue,
                                        FPOptionsOverride());
    }

    ImplicitCastExpr* decay_to_function_ptr(QualType fn_type, Expr* expr) {
        return ImplicitCastExpr::Create(ctx,
                                        ctx.getPointerType(fn_type),
                                        CK_FunctionToPointerDecay,
                                        expr,
                                        nullptr,
                                        VK_PRValue,
                                        FPOptionsOverride());
    }

    DeclRefExpr* fn_decl_ref(Loc loc, FunctionDecl* func) {
        return DeclRefExpr::Create(ctx,
                                   NestedNameSpecifierLoc(),
                                   Loc(),
                                   func,
                                   false,
                                   loc,
                                   func->getType(),
                                   VK_LValue);
    }

    DeclRefExpr* var_decl_ref(Loc loc, VarDecl* var) {
        return DeclRefExpr::Create(ctx,
                                   NestedNameSpecifierLoc(),
                                   Loc(),
                                   var,
                                   false,
                                   loc,
                                   var->getType(),
                                   VK_LValue);
    }

    DeclRefExpr* const_decl_ref(Loc loc, VarDecl* var) {
        return DeclRefExpr::Create(ctx,
                                   NestedNameSpecifierLoc(),
                                   Loc(),
                                   var,
                                   false,
                                   loc,
                                   ctx.getConstType(var->getType()),
                                   VK_LValue);
    }

    auto get_noexcept_ext_proto_info() -> FunctionProtoType::ExtProtoInfo {
        return FunctionProtoType::ExtProtoInfo().withExceptionSpec(
            FunctionProtoType::ExceptionSpecInfo(ExceptionSpecificationType::EST_BasicNoexcept));
    }

    auto builtin_alloca_decl(Loc loc) -> FunctionDecl* {
        auto            name      = ctx.BuiltinInfo.getName(Builtin::ID::BI__builtin_alloca);
        DeclarationName decl_name = &ctx.Idents.get(name);

        auto lookup_result = ctx.getTranslationUnitDecl()->lookup(decl_name);

        if (auto func_decl = lookup_result.find_first<FunctionDecl>()) {
            return func_decl;
        }

        // Create the builtin, if it does not exist
        auto& sema = compiler.getSema();

        auto ty = ctx.getFunctionType(ctx.VoidPtrTy,
                                      array_ref<QualType>({ctx.getSizeType()}),
                                      get_noexcept_ext_proto_info());

        IdentifierInfo* info = &ctx.Idents.get(name);
        return sema.CreateBuiltin(info, ty, Builtin::ID::BI__builtin_alloca, loc);
    }

    auto builtin_alloca(Loc loc, size_t bytes) -> Expr* {
        auto func_decl = builtin_alloca_decl(loc); // Get the declaration for __builtin_alloca

        auto fn_ptr = decay_to_function_ptr(func_decl->getType(), fn_decl_ref(loc, func_decl));

        return CallExpr::Create(ctx,
                                fn_ptr,
                                array_ref<Expr*>({size_literal(loc, bytes)}),
                                ctx.VoidPtrTy,
                                VK_PRValue,
                                loc,
                                FPOptionsOverride());
    }

    /// Get a function pointer expression from a function declaration
    ImplicitCastExpr* fn_ptr(Loc loc, FunctionDecl* func) {
        return decay_to_function_ptr(func->getType(), fn_decl_ref(loc, func));
    }

    UnaryOperator* deref(Loc loc, QualType type, Expr* expr, bool can_overflow = false) {
        return UnaryOperator::Create(ctx,
                                     expr,
                                     UO_Deref,
                                     type,
                                     VK_LValue,
                                     OK_Ordinary,
                                     loc,
                                     can_overflow,
                                     FPOptionsOverride());
    }

    CXXThisExpr* this_expr(Loc loc, QualType type, bool is_implicit = false) {
        return CXXThisExpr::Create(ctx, loc, ctx.getPointerType(type), is_implicit);
    }

    auto op_postfix_inc(Loc loc, Expr* expr, QualType type) -> UnaryOperator* {
        return UnaryOperator::Create(ctx,
                                     expr,
                                     UnaryOperator::Opcode::UO_PostInc,
                                     type,
                                     ExprValueKind::VK_PRValue,
                                     ExprObjectKind::OK_Ordinary,
                                     loc,
                                     true, // TODO: should canOverflow be true?
                                     FPOptionsOverride());
    }

    auto declare_static_var(Loc loc, DeclContext* decl_context, StringRef name, QualType type)
        -> VarDecl* {
        return VarDecl::Create(
            ctx,
            decl_context,
            loc,
            loc,
            &ctx.Idents.get(name), // If the identifier does not exist, it shall be created
            type,
            ctx.getTrivialTypeSourceInfo(type, loc),
            clang::SC_Static);
    }

    auto make_decl_stmt(Loc loc, Decl* decl) -> DeclStmt* {
        return new (ctx) DeclStmt(DeclGroupRef::Create(ctx, &decl, 1), loc, loc);
    }


    auto deref_this(Loc loc, QualType type) -> UnaryOperator* {
        return deref(loc, type, this_expr(loc, type));
    }

    auto sizeof_type(Loc loc, QualType type) -> UnaryExprOrTypeTraitExpr* {
        return new (ctx) UnaryExprOrTypeTraitExpr(UETT_SizeOf,
                                                  ctx.getTrivialTypeSourceInfo(type),
                                                  ctx.getSizeType(),
                                                  loc,
                                                  loc);
    }

    auto sizeof_this(Loc loc, QualType type) -> UnaryExprOrTypeTraitExpr* {
        return new (ctx) UnaryExprOrTypeTraitExpr(UETT_SizeOf,
                                                  deref_this(loc, type),
                                                  ctx.getSizeType(),
                                                  loc,
                                                  loc);
    }

    auto string_literal(Loc loc, StringRef s) -> StringLiteral* {
        // Note: in instances I've seen in the clang codebase, StringLiteral::Create() is perfectly fine
        // accepting a std::string (allocated on the stack). This implies to me that `StringLiteral::Create`
        // makes a copy of the input string.
        return StringLiteral::Create(ctx,
                                     s,
                                     StringLiteralKind::Ordinary,
                                     false,
                                     ctx.getStringLiteralArrayType(ctx.CharTy, s.size()),
                                     loc);
    }

    auto to_atomic(Expr* input) -> ImplicitCastExpr* {
        return ImplicitCastExpr::Create(ctx,
                                        ctx.getAtomicType(input->getType()),
                                        CastKind::CK_NonAtomicToAtomic,
                                        input,
                                        nullptr,
                                        ExprValueKind::VK_PRValue,
                                        FPOptionsOverride());
    }

    auto to_char_ptr(StringLiteral* literal) -> ImplicitCastExpr* {

        return ImplicitCastExpr::Create(ctx,
                                        ctx.getArrayDecayedType(literal->getType()),
                                        CastKind::CK_ArrayToPointerDecay,
                                        literal,
                                        nullptr,
                                        VK_PRValue,
                                        FPOptionsOverride());
    }

    auto size_literal(Loc loc, size_t literal_value) -> IntegerLiteral* {
        auto   size_type   = ctx.getSizeType();
        size_t size_t_bits = ctx.getTypeSize(size_type);
        return IntegerLiteral::Create(ctx, llvm::APInt(size_t_bits, literal_value), size_type, loc);
    }

    auto ull_literal(Loc loc, unsigned long long literal_value) -> IntegerLiteral* {
        auto   type      = ctx.UnsignedLongLongTy;
        size_t bit_width = ctx.getTypeSize(type);
        return IntegerLiteral::Create(ctx, llvm::APInt(bit_width, literal_value), type, loc);
    }

    auto invoke_hook(Loc            loc,
                     QualType       type,
                     FunctionDecl*  hook_decl,
                     CXXRecordDecl* record,
                     CXXMethodDecl* method_ctx) -> std::array<Stmt*, 2> {
        // Get a function pointer to the hook
        auto func = fn_ptr(loc, hook_decl);

        const ASTRecordLayout& layout = ctx.getASTRecordLayout(record);

        auto const& bases_  = record->bases();
        auto const& fields_ = record->fields();

        size_t base_count  = bases_.end() - bases_.begin();
        size_t field_count = layout.getFieldCount();


        auto mp_type_data_decl = find_record_decl("_mp_type_data");
        if (!mp_type_data_decl->isCompleteDefinition()) {
            throw std::runtime_error("Complete definition required");
        }

        auto mp_type_data_qual_type = ctx.getTypeDeclType(mp_type_data_decl);
        auto type_data
            = declare_static_var(loc, method_ctx, "_mp_TYPE_DATA", mp_type_data_qual_type);
        type_data->setConstexpr(true);

        auto init_expr
            = new (ctx) InitListExpr(ctx,
                                     loc,
                                     array_ref<Expr*>({
                                         // size_literal(loc, layout.getSize().getQuantity()),
                                         sizeof_type(loc, type),
                                         to_char_ptr(string_literal(loc, type.getAsString(pol))),
                                         size_literal(loc, base_count),
                                         size_literal(loc, field_count),
                                     }),
                                     loc);
        init_expr->setType(ctx.getConstType(mp_type_data_qual_type));

        type_data->setInit(init_expr);

        auto args = array_ref<Expr*>({
            to_void_ptr(this_expr(loc, type)),
            builtin_alloca(loc, 40),
            const_decl_ref(loc, type_data),
        });



        // // Print member variables and their types
        // llvm::outs() << "type: " << type.getAsString(pol) << "\n";



        // for (auto base : record->bases()) {
        //     CharUnits offset = layout.getBaseClassOffset(base.getType()->getAsCXXRecordDecl());
        //     llvm::outs() << "- base: " << base.getType().getAsString(pol)
        //                  << "\n  offset: " << offset.getQuantity() << "\n";
        // }

        // for (auto field : record->fields()) {
        //     uint64_t offset_bits  = layout.getFieldOffset(field->getFieldIndex());
        //     uint64_t offset_bytes = offset_bits / 8;
        //     llvm::outs() << "- member: " << field->getNameAsString()
        //                  << "\n  type: " << field->getType().getAsString(pol)
        //                  << "\n  offset: " << offset_bytes << "\n";
        // }

        auto call_expr
            = CallExpr::Create(ctx, func, args, ctx.VoidTy, VK_PRValue, loc, FPOptionsOverride());

        auto decl_stmt = create_decl_stmt(loc, type_data);
        return std::array<Stmt*, 2>{
            decl_stmt,
            call_expr,
        };
    }

    auto create_decl_stmt(Loc loc, VarDecl* var_decl) -> DeclStmt* {
        auto decls = mut_array_ref<Decl*>({var_decl});

        return new (ctx) DeclStmt(DeclGroupRef::Create(ctx, decls.data(), decls.size()), loc, loc);
    }

    auto invoke_hook_with_counter(Loc           loc,
                                  QualType      type,
                                  FunctionDecl* hook_decl,
                                  DeclContext*  decl_context) -> std::array<Stmt*, 2> {
        auto var_decl = declare_static_var(loc,
                                           decl_context,
                                           "__mem_profile_counter",
                                           ctx.getAtomicType(ctx.UnsignedLongLongTy));

        var_decl->setInit(to_atomic(ull_literal(loc, 0)));

        auto decls = mut_array_ref<Decl*>({var_decl});

        // Get a function pointer to the hooku
        auto func = fn_ptr(loc, hook_decl);

        auto args = array_ref<Expr*>(
            {to_void_ptr(this_expr(loc, type)),
             builtin_alloca(loc, 48),
             sizeof_type(loc, type),
             to_char_ptr(string_literal(loc, type.getAsString(pol))),
             op_postfix_inc(loc, var_decl_ref(loc, var_decl), ctx.UnsignedLongLongTy)});

        return {
            new (ctx) DeclStmt(DeclGroupRef::Create(ctx, decls.data(), decls.size()), loc, loc),
            CallExpr::Create(ctx, func, args, ctx.VoidTy, VK_PRValue, loc, FPOptionsOverride())};
    }
};

} // namespace mp
