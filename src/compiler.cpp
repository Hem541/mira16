#include "compiler.h"
#include <sstream>
#include <unordered_map>
#include <cctype>
#include <memory>
#include <vector>
#include <stdexcept>

// minic compiler. lexer -> recursive-descent parser (AST) -> codegen.

namespace {

// stage 1: lexer. source string -> flat token list.
enum class Tok {
    End, Num, Ident,
    Var, If, Else, While, Print,
    Plus, Minus, Star, Slash,
    Assign,                 // =
    Lt, Gt, Eq, Ne, Le, Ge, // < > == != <= >=
    LParen, RParen, LBrace, RBrace, Semi
};

struct Token {
    Tok type;
    std::string text;   // for Ident
    int value = 0;      // for Num
    int line = 1;
};

struct Lexer {
    std::string s;
    size_t i = 0;
    int line = 1;
    std::vector<std::string>* errors;

    explicit Lexer(const std::string& src, std::vector<std::string>* errs)
        : s(src), errors(errs) {}

    std::vector<Token> run() {
        std::vector<Token> out;
        while (true) {
            skipSpaceAndComments();
            if (i >= s.size()) { out.push_back({Tok::End,"",0,line}); break; }
            char c = s[i];
            if (std::isdigit((unsigned char)c)) { out.push_back(number()); continue; }
            if (std::isalpha((unsigned char)c) || c=='_') { out.push_back(word()); continue; }
            out.push_back(symbol());
        }
        return out;
    }

    void skipSpaceAndComments() {
        while (i < s.size()) {
            char c = s[i];
            if (c=='\n') { ++line; ++i; }
            else if (std::isspace((unsigned char)c)) { ++i; }
            else if (c=='/' && i+1<s.size() && s[i+1]=='/') { // line comment
                while (i<s.size() && s[i]!='\n') ++i;
            } else break;
        }
    }

    Token number() {
        int start = (int)i; int v = 0;
        while (i<s.size() && std::isdigit((unsigned char)s[i])) {
            v = v*10 + (s[i]-'0'); ++i;
        }
        (void)start;
        return {Tok::Num,"",v,line};
    }

    Token word() {
        std::string w;
        while (i<s.size() && (std::isalnum((unsigned char)s[i])||s[i]=='_')) w+=s[i++];
        if (w=="var")   return {Tok::Var,w,0,line};
        if (w=="if")    return {Tok::If,w,0,line};
        if (w=="else")  return {Tok::Else,w,0,line};
        if (w=="while") return {Tok::While,w,0,line};
        if (w=="print") return {Tok::Print,w,0,line};
        return {Tok::Ident,w,0,line};
    }

    Token symbol() {
        char c = s[i];
        auto two = [&](char a, char b){ return s[i]==a && i+1<s.size() && s[i+1]==b; };
        if (two('=','=')) { i+=2; return {Tok::Eq,"==",0,line}; }
        if (two('!','=')) { i+=2; return {Tok::Ne,"!=",0,line}; }
        if (two('<','=')) { i+=2; return {Tok::Le,"<=",0,line}; }
        if (two('>','=')) { i+=2; return {Tok::Ge,">=",0,line}; }
        ++i;
        switch (c) {
            case '+': return {Tok::Plus,"+",0,line};
            case '-': return {Tok::Minus,"-",0,line};
            case '*': return {Tok::Star,"*",0,line};
            case '/': return {Tok::Slash,"/",0,line};
            case '=': return {Tok::Assign,"=",0,line};
            case '<': return {Tok::Lt,"<",0,line};
            case '>': return {Tok::Gt,">",0,line};
            case '(': return {Tok::LParen,"(",0,line};
            case ')': return {Tok::RParen,")",0,line};
            case '{': return {Tok::LBrace,"{",0,line};
            case '}': return {Tok::RBrace,"}",0,line};
            case ';': return {Tok::Semi,";",0,line};
        }
        if (errors) errors->push_back("line "+std::to_string(line)+
                                      ": unexpected character '"+std::string(1,c)+"'");
        return {Tok::End,"",0,line};
    }
};

// stage 2: parser (recursive descent). tokens -> AST.
//  grammar (precedence built in):
//
//    program   := stmt*
//    stmt      := 'var' ident '=' expr ';'
//               | ident '=' expr ';'
//               | 'print' expr ';'
//               | 'if' '(' cond ')' block ('else' block)?
//               | 'while' '(' cond ')' block
//    block     := '{' stmt* '}'
//    cond      := expr relop expr
//    expr      := term (('+'|'-') term)*
//    term      := factor (('*'|'/') factor)*
//    factor    := number | ident | '(' expr ')'
//
//  recursive descent = one function per rule. call structure mirrors the grammar.

// --- AST node types ---
struct Expr { virtual ~Expr() = default; };
using ExprP = std::unique_ptr<Expr>;

struct NumExpr : Expr { int value; explicit NumExpr(int v):value(v){} };
struct VarExpr : Expr { std::string name; explicit VarExpr(std::string n):name(std::move(n)){} };
struct BinExpr : Expr { char op; ExprP l,r; BinExpr(char o,ExprP a,ExprP b):op(o),l(std::move(a)),r(std::move(b)){} };

struct Stmt { virtual ~Stmt() = default; };
using StmtP = std::unique_ptr<Stmt>;

struct VarDecl : Stmt { std::string name; ExprP init; };
struct Assign  : Stmt { std::string name; ExprP value; };
struct Print   : Stmt { ExprP value; };
struct Cond    : Stmt {}; // base for if/while sharing a comparison

struct IfStmt : Stmt {
    ExprP l, r; Tok rel;
    std::vector<StmtP> thenBody, elseBody;
    bool hasElse=false;
};
struct WhileStmt : Stmt {
    ExprP l, r; Tok rel;
    std::vector<StmtP> body;
};

struct ParseError : std::runtime_error {
    explicit ParseError(const std::string& m):std::runtime_error(m){}
};

struct Parser {
    std::vector<Token> toks;
    size_t p = 0;

    explicit Parser(std::vector<Token> t):toks(std::move(t)){}

    const Token& peek() const { return toks[p]; }
    const Token& get()  { return toks[p++]; }
    bool is(Tok t) const { return toks[p].type==t; }
    Token expect(Tok t, const char* what) {
        if (!is(t)) throw ParseError("line "+std::to_string(peek().line)+
                                     ": expected "+what);
        return get();
    }

    std::vector<StmtP> parseProgram() {
        std::vector<StmtP> prog;
        while (!is(Tok::End)) prog.push_back(parseStmt());
        return prog;
    }

    StmtP parseStmt() {
        if (is(Tok::Var))   return parseVarDecl();
        if (is(Tok::Print)) return parsePrint();
        if (is(Tok::If))    return parseIf();
        if (is(Tok::While)) return parseWhile();
        if (is(Tok::Ident)) return parseAssign();
        throw ParseError("line "+std::to_string(peek().line)+
                         ": expected a statement");
    }

    StmtP parseVarDecl() {
        get(); // 'var'
        auto name = expect(Tok::Ident,"variable name").text;
        expect(Tok::Assign,"'='");
        auto e = parseExpr();
        expect(Tok::Semi,"';'");
        auto d = std::make_unique<VarDecl>(); d->name=name; d->init=std::move(e);
        return d;
    }

    StmtP parseAssign() {
        auto name = get().text; // ident
        expect(Tok::Assign,"'='");
        auto e = parseExpr();
        expect(Tok::Semi,"';'");
        auto a = std::make_unique<Assign>(); a->name=name; a->value=std::move(e);
        return a;
    }

    StmtP parsePrint() {
        get(); // 'print'
        auto e = parseExpr();
        expect(Tok::Semi,"';'");
        auto pr = std::make_unique<Print>(); pr->value=std::move(e);
        return pr;
    }

    std::vector<StmtP> parseBlock() {
        expect(Tok::LBrace,"'{'");
        std::vector<StmtP> body;
        while (!is(Tok::RBrace) && !is(Tok::End)) body.push_back(parseStmt());
        expect(Tok::RBrace,"'}'");
        return body;
    }

    bool isRelop(Tok t) const {
        return t==Tok::Lt||t==Tok::Gt||t==Tok::Eq||t==Tok::Ne||t==Tok::Le||t==Tok::Ge;
    }

    StmtP parseIf() {
        get(); // 'if'
        expect(Tok::LParen,"'('");
        auto l = parseExpr();
        if (!isRelop(peek().type))
            throw ParseError("line "+std::to_string(peek().line)+": expected comparison");
        Tok rel = get().type;
        auto r = parseExpr();
        expect(Tok::RParen,"')'");
        auto s = std::make_unique<IfStmt>();
        s->l=std::move(l); s->r=std::move(r); s->rel=rel;
        s->thenBody = parseBlock();
        if (is(Tok::Else)) { get(); s->elseBody=parseBlock(); s->hasElse=true; }
        return s;
    }

    StmtP parseWhile() {
        get(); // 'while'
        expect(Tok::LParen,"'('");
        auto l = parseExpr();
        if (!isRelop(peek().type))
            throw ParseError("line "+std::to_string(peek().line)+": expected comparison");
        Tok rel = get().type;
        auto r = parseExpr();
        expect(Tok::RParen,"')'");
        auto s = std::make_unique<WhileStmt>();
        s->l=std::move(l); s->r=std::move(r); s->rel=rel;
        s->body = parseBlock();
        return s;
    }

    // expr := term (('+'|'-') term)*
    ExprP parseExpr() {
        auto l = parseTerm();
        while (is(Tok::Plus)||is(Tok::Minus)) {
            char op = is(Tok::Plus)?'+':'-'; get();
            auto r = parseTerm();
            l = std::make_unique<BinExpr>(op,std::move(l),std::move(r));
        }
        return l;
    }
    // term := factor (('*'|'/') factor)*
    ExprP parseTerm() {
        auto l = parseFactor();
        while (is(Tok::Star)||is(Tok::Slash)) {
            char op = is(Tok::Star)?'*':'/'; get();
            auto r = parseFactor();
            l = std::make_unique<BinExpr>(op,std::move(l),std::move(r));
        }
        return l;
    }
    // factor := number | ident | '(' expr ')'
    ExprP parseFactor() {
        if (is(Tok::Num))   return std::make_unique<NumExpr>(get().value);
        if (is(Tok::Ident)) return std::make_unique<VarExpr>(get().text);
        if (is(Tok::LParen)){ get(); auto e=parseExpr(); expect(Tok::RParen,"')'"); return e; }
        throw ParseError("line "+std::to_string(peek().line)+
                         ": expected number, variable, or '('");
    }
};

// stage 3: codegen. walk AST -> emit assembly.
//    - each variable gets a fixed memory address (a slot).
//    - Expressions are evaluated into R1 using a tiny stack discipline:
//      compute left into R1, PUSH it, compute right into R1, POP left
//      into R2, combine. This handles arbitrary nesting correctly.
//    - if/while emit CMP + conditional jumps to generated labels.
struct CodeGen {
    std::ostringstream out;
    std::unordered_map<std::string,int> varSlot; // name -> memory address
    int nextSlot = 200;       // variables live at mem[200], 201, ...
    int labelId = 0;
    std::vector<std::string>* errors;

    explicit CodeGen(std::vector<std::string>* errs):errors(errs){}

    std::string newLabel(const std::string& base){
        return base + std::to_string(labelId++);
    }
    int slotOf(const std::string& name){
        auto it = varSlot.find(name);
        if (it!=varSlot.end()) return it->second;
        int s = nextSlot++; varSlot[name]=s; return s;
    }

    void emit(const std::string& line){ out<<line<<"\n"; }

    // Evaluate expr, leaving its result in R1.
    void genExpr(Expr* e){
        if (auto n=dynamic_cast<NumExpr*>(e)){
            emit("LOADI R1, "+std::to_string(n->value & 0xFF));
            return;
        }
        if (auto v=dynamic_cast<VarExpr*>(e)){
            emit("LOAD R1, ["+std::to_string(slotOf(v->name))+"]");
            return;
        }
        if (auto b=dynamic_cast<BinExpr*>(e)){
            genExpr(b->l.get());          // left -> R1
            emit("PUSH R1");              // save left
            genExpr(b->r.get());          // right -> R1
            emit("MOV R3, R1");           // right -> R3
            emit("POP R2");               // left -> R2
            switch (b->op){
                case '+': emit("ADD R1, R2, R3"); break;
                case '-': emit("SUB R1, R2, R3"); break;
                case '*': emit("MUL R1, R2, R3"); break;
                case '/': emit("DIV R1, R2, R3"); break;
            }
            return;
        }
    }

    // Emit the comparison + a conditional jump to `target` taken when the
    // condition is FALSE (so control falls through when TRUE).
    void genCondFalseJump(Expr* l, Tok rel, Expr* r, const std::string& target){
        genExpr(l); emit("PUSH R1");
        genExpr(r); emit("MOV R3, R1"); emit("POP R2");
        // R2 = left, R3 = right. CMP sets flags from (R2 - R3).
        emit("CMP R2, R3");
        // We branch to `target` when condition is false. Our ISA only has
        // JZ/JNZ (zero flag). We can directly handle == and !=; for the
        // ordering relations we use SUB + sign, but to keep MiniC simple
        // and CORRECT we implement all six via zero/!zero of a computed
        // boolean in R4 (1 = condition true, 0 = false), then JZ target.
        // Build R4 = (condition ? 1 : 0):
        std::string setTrue = newLabel("ct");
        std::string done    = newLabel("cd");
        // Recompute relation explicitly for clarity:
        // subtract gives flags; we translate each relop into JZ/JNZ logic.
        // Simplest robust approach: compute difference d = left - right.
        // We already have CMP flags but only zero is reliable across signs
        // for our purposes here, so for < > <= >= we use SUB and the sign
        // bit via the negative flag is not exposed to jumps; instead we
        // compute booleans arithmetically below.
        (void)setTrue; (void)done;

        // Arithmetic boolean construction (no reliance on signed flags):
        // We'll compute into R4 a 1/0 truth value, then JZ target when 0.
        // left in R2, right in R3 still valid.
        switch (rel){
            case Tok::Eq: // left == right  -> (left - right) == 0
                emit("SUB R4, R2, R3");
                emit("CMP R4, R0");        // R0 is always 0
                emit("JNZ "+target);       // not equal -> condition false
                return;
            case Tok::Ne: // left != right
                emit("SUB R4, R2, R3");
                emit("CMP R4, R0");
                emit("JZ "+target);        // equal -> condition false
                return;
            default: break;
        }
        // Ordering relations: use repeated subtraction-free trick is hard
        // without signed compares, so we synthesize using SUB and checking
        // which side is larger via a subtract-and-test loop-free method:
        //   diff = left - right (16-bit). If left >= right and no wrap,
        //   diff's high bit (bit15) is 0; if left < right, subtract wraps
        //   and bit15 is 1. We expose that by shifting right 15.
        // sign = (left - right) >> 15   -> 1 if left<right else 0
        emit("SUB R4, R2, R3");           // R4 = left - right (wraps if neg)
        emit("LOADI R5, 15");
        emit("SHR R4, R4, R5");           // R4 = sign bit (1 if left<right)
        // Now R4 = 1 when left<right, else 0.
        switch (rel){
            case Tok::Lt: // true when left<right (R4==1); false when R4==0
                emit("CMP R4, R0"); emit("JZ "+target); return;
            case Tok::Ge: // true when left>=right (R4==0); false when R4==1
                emit("CMP R4, R0"); emit("JNZ "+target); return;
            case Tok::Gt: { // left>right == right<left
                // recompute sign for (right<left): we already have R4 for
                // (left<right). right<left means (right-left) sign:
                emit("SUB R4, R3, R2");
                emit("LOADI R5, 15"); emit("SHR R4, R4, R5");
                emit("CMP R4, R0"); emit("JZ "+target); return;
            }
            case Tok::Le: { // left<=right == !(right<left)
                emit("SUB R4, R3, R2");
                emit("LOADI R5, 15"); emit("SHR R4, R4, R5");
                emit("CMP R4, R0"); emit("JNZ "+target); return;
            }
            default: return;
        }
    }

    void genStmt(Stmt* s){
        if (auto d=dynamic_cast<VarDecl*>(s)){
            genExpr(d->init.get());
            emit("STORE R1, ["+std::to_string(slotOf(d->name))+"]");
            return;
        }
        if (auto a=dynamic_cast<Assign*>(s)){
            genExpr(a->value.get());
            emit("STORE R1, ["+std::to_string(slotOf(a->name))+"]");
            return;
        }
        if (auto p=dynamic_cast<Print*>(s)){
            genExpr(p->value.get());
            emit("OUT R1");
            return;
        }
        if (auto f=dynamic_cast<IfStmt*>(s)){
            std::string elseL = newLabel("else");
            std::string endL  = newLabel("endif");
            genCondFalseJump(f->l.get(), f->rel, f->r.get(),
                             f->hasElse?elseL:endL);
            for (auto& st: f->thenBody) genStmt(st.get());
            if (f->hasElse){
                emit("JMP "+endL);
                emit(elseL+":");
                for (auto& st: f->elseBody) genStmt(st.get());
            }
            emit(endL+":");
            return;
        }
        if (auto w=dynamic_cast<WhileStmt*>(s)){
            std::string topL = newLabel("while");
            std::string endL = newLabel("endwhile");
            emit(topL+":");
            genCondFalseJump(w->l.get(), w->rel, w->r.get(), endL);
            for (auto& st: w->body) genStmt(st.get());
            emit("JMP "+topL);
            emit(endL+":");
            return;
        }
    }

    std::string run(std::vector<StmtP>& prog){
        for (auto& s: prog) genStmt(s.get());
        emit("HALT");
        return out.str();
    }
};

} // anonymous namespace

CompileResult compile(const std::string& source){
    CompileResult res;
    Lexer lex(source, &res.errors);
    auto toks = lex.run();
    if (!res.errors.empty()) return res;

    try {
        Parser parser(std::move(toks));
        auto prog = parser.parseProgram();
        CodeGen cg(&res.errors);
        res.asmText = cg.run(prog);
    } catch (const ParseError& e) {
        res.errors.push_back(e.what());
    }
    return res;
}
