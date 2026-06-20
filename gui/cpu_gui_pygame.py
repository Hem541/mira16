#!/usr/bin/env python3
# pygame gui for the 16-bit cpu. mac/win/linux. run: python3 cpu_gui_pygame.py
# cpu logic is a python port of the c++ core (same isa, same results).

import pygame
import sys

# cpu core. mirrors the c++ implementation (same isa, same results).
OP = dict(NOP=0,LOADI=1,LOAD=2,STORE=3,MOV=4,ADD=5,SUB=6,AND=7,OR=8,XOR=9,
          NOT=10,SHL=11,SHR=12,CMP=13,JMP=14,JZ=15,JNZ=16,PUSH=17,POP=18,
          HALT=19,MUL=20,DIV=21,CALL=22,RET=23,OUT=24)
OPNAME={v:k for k,v in OP.items()}
def fmt_of(op):
    if op in (4,5,6,7,8,9,10,11,12,13,20,21): return 'R'
    if op in (1,2,3,17,18,24): return 'I'
    if op in (14,15,16,22): return 'J'
    return 'N'
def eR(o,d,s,t): return ((o&31)<<11)|((d&7)<<8)|((s&7)<<5)|((t&7)<<2)
def eI(o,d,i):   return ((o&31)<<11)|((d&7)<<8)|(i&255)
def eJ(o,a):     return ((o&31)<<11)|(a&2047)
def eN(o):       return (o&31)<<11
def decode(w):
    op=(w>>11)&31
    return dict(op=op,fmt=fmt_of(op),rd=(w>>8)&7,rs=(w>>5)&7,rt=(w>>2)&7,
                imm=w&255,addr=w&2047,raw=w)
def assemble(src):
    errors=[]; lines=[l.split(';',1)[0] for l in src.split('\n')]; MN=dict(OP)
    def rn(t):
        if not t or t[0] not in 'Rr': return -1
        try: return int(t[1:])
        except: return -1
    labels={}; prog=[]; addr=0
    for i,raw in enumerate(lines):
        t=raw.replace(',',' ').split()
        while t and t[0].endswith(':'): labels[t[0][:-1].upper()]=addr; t=t[1:]
        if not t: continue
        prog.append((t,i+1)); addr+=1
    def res(x):
        if x.startswith('['): x=x[1:]
        if x.endswith(']'): x=x[:-1]
        try: return int(x,0)
        except: pass
        return labels.get(x.upper(),None)
    code=[]
    for t,ln in prog:
        m=t[0].upper()
        if m not in MN: errors.append(f"line {ln}: unknown instruction '{t[0]}'"); code.append(0); continue
        op=MN[m]; f=fmt_of(op); w=0
        def fail(s): errors.append(f"line {ln}: {s}")
        if f=='N': w=eN(op)
        elif f=='R':
            if op in (10,4):
                d=rn(t[1]) if len(t)>1 else -1; s=rn(t[2]) if len(t)>2 else -1
                if d<0 or s<0: fail("expected 'rd, rs'")
                w=eR(op,max(d,0),max(s,0),0)
            elif op==13:
                s=rn(t[1]) if len(t)>1 else -1; tt=rn(t[2]) if len(t)>2 else -1
                if s<0 or tt<0: fail("expected 'rs, rt'")
                w=eR(op,0,max(s,0),max(tt,0))
            else:
                d=rn(t[1]) if len(t)>1 else -1; s=rn(t[2]) if len(t)>2 else -1; tt=rn(t[3]) if len(t)>3 else -1
                if d<0 or s<0 or tt<0: fail("expected 'rd, rs, rt'")
                w=eR(op,max(d,0),max(s,0),max(tt,0))
        elif f=='I':
            d=rn(t[1]) if len(t)>1 else -1
            if d<0: fail("expected register")
            im=0
            if op in (17,18,24): im=0
            else:
                r=res(t[2]) if len(t)>2 else None
                if r is None: fail("expected immediate")
                else: im=r
            w=eI(op,max(d,0),im&255)
        elif f=='J':
            a=res(t[1]) if len(t)>1 else None
            if a is None: fail("expected jump target"); a=0
            w=eJ(op,a)
        code.append(w)
    return code,errors
def disasm(i):
    R=lambda x:f"R{x}"; o=i['op']
    d={0:"NOP",19:"HALT",23:"RET"}
    if o in d: return d[o]
    if o==1: return f"LOADI {R(i['rd'])}, {i['imm']}"
    if o==2: return f"LOAD {R(i['rd'])}, [{i['imm']}]"
    if o==3: return f"STORE {R(i['rd'])}, [{i['imm']}]"
    if o==4: return f"MOV {R(i['rd'])}, {R(i['rs'])}"
    if o in (5,6,7,8,11,12,20,21):
        nm={5:"ADD",6:"SUB",7:"AND",8:"OR",11:"SHL",12:"SHR",20:"MUL",21:"DIV"}[o]
        return f"{nm} {R(i['rd'])}, {R(i['rs'])}, {R(i['rt'])}"
    if o==9: return f"XOR {R(i['rd'])}, {R(i['rs'])}, {R(i['rt'])}"
    if o==10: return f"NOT {R(i['rd'])}, {R(i['rs'])}"
    if o==13: return f"CMP {R(i['rs'])}, {R(i['rt'])}"
    if o in (14,15,16,22):
        nm={14:"JMP",15:"JZ",16:"JNZ",22:"CALL"}[o]; return f"{nm} {i['addr']}"
    if o==17: return f"PUSH {R(i['rd'])}"
    if o==18: return f"POP {R(i['rd'])}"
    if o==24: return f"OUT {R(i['rd'])}"
    return "?"
#  MiniC COMPILER  (Python port of the C++ compiler.cpp -- same output)
#  Compiles a small C-like language to this ISA's assembly.
def compile_minic(src):
    errors=[]; KW={'var','if','else','while','print'}; toks=[]
    i=0; n=len(src); line=1
    while i<n:
        c=src[i]
        if c=='\n': line+=1; i+=1; continue
        if c in ' \t\r': i+=1; continue
        if c=='/' and i+1<n and src[i+1]=='/':
            while i<n and src[i]!='\n': i+=1
            continue
        if c.isdigit():
            v=0
            while i<n and src[i].isdigit(): v=v*10+int(src[i]); i+=1
            toks.append(('num','',v,line)); continue
        if c.isalpha() or c=='_':
            w=''
            while i<n and (src[i].isalnum() or src[i]=='_'): w+=src[i]; i+=1
            toks.append((w if w in KW else 'ident',w,0,line)); continue
        two=src[i:i+2]
        if two in ('==','!=','<=','>='): toks.append((two,two,0,line)); i+=2; continue
        if c in '+-*/=<>(){};': toks.append((c,c,0,line)); i+=1; continue
        errors.append(f"line {line}: unexpected character '{c}'"); i+=1
    toks.append(('eof','',0,line))
    if errors: return None,errors
    pos=[0]
    def peek(): return toks[pos[0]]
    def get(): t=toks[pos[0]]; pos[0]+=1; return t
    def is_(ty): return toks[pos[0]][0]==ty
    class PErr(Exception): pass
    def expect(ty,what):
        if not is_(ty): raise PErr(f"line {peek()[3]}: expected {what}")
        return get()
    RELOPS={'<','>','==','!=','<=','>='}
    def parseFactor():
        if is_('num'): return ('num',get()[2])
        if is_('ident'): return ('var',get()[1])
        if is_('('):
            get(); e=parseExpr(); expect(')',"')'"); return e
        raise PErr(f"line {peek()[3]}: expected number, variable, or '('")
    def parseTerm():
        l=parseFactor()
        while is_('*') or is_('/'): op=get()[0]; r=parseFactor(); l=('bin',op,l,r)
        return l
    def parseExpr():
        l=parseTerm()
        while is_('+') or is_('-'): op=get()[0]; r=parseTerm(); l=('bin',op,l,r)
        return l
    def parseBlock():
        expect('{',"'{'"); body=[]
        while not is_('}') and not is_('eof'): body.append(parseStmt())
        expect('}',"'}'"); return body
    def parseStmt():
        t=peek()[0]
        if t=='var':
            get(); name=expect('ident','variable name')[1]
            expect('=',"'='"); e=parseExpr(); expect(';',"';'"); return ('decl',name,e)
        if t=='print':
            get(); e=parseExpr(); expect(';',"';'"); return ('print',e)
        if t=='if':
            get(); expect('(',"'('"); l=parseExpr()
            if peek()[0] not in RELOPS: raise PErr(f"line {peek()[3]}: expected comparison")
            rel=get()[0]; r=parseExpr(); expect(')',"')'")
            then=parseBlock(); els=[]; has=False
            if is_('else'): get(); els=parseBlock(); has=True
            return ('if',l,rel,r,then,els,has)
        if t=='while':
            get(); expect('(',"'('"); l=parseExpr()
            if peek()[0] not in RELOPS: raise PErr(f"line {peek()[3]}: expected comparison")
            rel=get()[0]; r=parseExpr(); expect(')',"')'")
            return ('while',l,rel,r,parseBlock())
        if t=='ident':
            name=get()[1]; expect('=',"'='"); e=parseExpr(); expect(';',"';'"); return ('assign',name,e)
        raise PErr(f"line {peek()[3]}: expected a statement")
    try:
        prog=[]
        while not is_('eof'): prog.append(parseStmt())
    except PErr as e:
        return None,[str(e)]
    out=[]; slots={}; nxt=[200]; lbl=[0]
    def emit(s): out.append(s)
    def newLabel(b): lbl[0]+=1; return f"{b}{lbl[0]-1}"
    def slotOf(nm):
        if nm not in slots: slots[nm]=nxt[0]; nxt[0]+=1
        return slots[nm]
    def genExpr(e):
        if e[0]=='num': emit(f"LOADI R1, {e[1]&0xFF}")
        elif e[0]=='var': emit(f"LOAD R1, [{slotOf(e[1])}]")
        elif e[0]=='bin':
            genExpr(e[2]); emit("PUSH R1"); genExpr(e[3]); emit("MOV R3, R1"); emit("POP R2")
            emit({'+':"ADD R1, R2, R3",'-':"SUB R1, R2, R3",'*':"MUL R1, R2, R3",'/':"DIV R1, R2, R3"}[e[1]])
    def genCondFalseJump(l,rel,r,target):
        genExpr(l); emit("PUSH R1"); genExpr(r); emit("MOV R3, R1"); emit("POP R2"); emit("CMP R2, R3")
        if rel=='==': emit("SUB R4, R2, R3"); emit("CMP R4, R0"); emit(f"JNZ {target}"); return
        if rel=='!=': emit("SUB R4, R2, R3"); emit("CMP R4, R0"); emit(f"JZ {target}"); return
        emit("SUB R4, R2, R3"); emit("LOADI R5, 15"); emit("SHR R4, R4, R5")
        if rel=='<': emit("CMP R4, R0"); emit(f"JZ {target}"); return
        if rel=='>=': emit("CMP R4, R0"); emit(f"JNZ {target}"); return
        if rel=='>': emit("SUB R4, R3, R2"); emit("LOADI R5, 15"); emit("SHR R4, R4, R5"); emit("CMP R4, R0"); emit(f"JZ {target}"); return
        if rel=='<=': emit("SUB R4, R3, R2"); emit("LOADI R5, 15"); emit("SHR R4, R4, R5"); emit("CMP R4, R0"); emit(f"JNZ {target}"); return
    def genStmt(s):
        if s[0]=='decl': genExpr(s[2]); emit(f"STORE R1, [{slotOf(s[1])}]")
        elif s[0]=='assign': genExpr(s[2]); emit(f"STORE R1, [{slotOf(s[1])}]")
        elif s[0]=='print': genExpr(s[1]); emit("OUT R1")
        elif s[0]=='if':
            _,l,rel,r,then,els,has=s
            elseL=newLabel("else"); endL=newLabel("endif")
            genCondFalseJump(l,rel,r, elseL if has else endL)
            for st in then: genStmt(st)
            if has:
                emit(f"JMP {endL}"); emit(f"{elseL}:")
                for st in els: genStmt(st)
            emit(f"{endL}:")
        elif s[0]=='while':
            _,l,rel,r,body=s
            topL=newLabel("while"); endL=newLabel("endwhile")
            emit(f"{topL}:"); genCondFalseJump(l,rel,r,endL)
            for st in body: genStmt(st)
            emit(f"JMP {topL}"); emit(f"{endL}:")
    for s in prog: genStmt(s)
    emit("HALT")
    # also return which slots map to which var names, for the memory panel
    return "\n".join(out), [], slots

class CPU:
    def __init__(self): self.reset()
    def reset(self):
        self.regs=[0]*8; self.mem=[0]*65536; self.pc=0; self.sp=65535
        self.fz=self.fn=self.fc=False; self.halted=False; self.cyc=0
        self.outputs=[]; self.hitReg=-1; self.hitMem=-1
    def load(self,code):
        for i,w in enumerate(code): self.mem[i]=w
        self.pc=0
    def w(self,i,v):
        if i==0: return
        self.regs[i]=v&0xFFFF; self.hitReg=i
    def sf(self,x):
        l=x&0xFFFF; self.fz=(l==0); self.fn=(l&0x8000)!=0; self.fc=(x&0x10000)!=0
    def step(self):
        if self.halted: return None
        self.hitReg=-1; self.hitMem=-1
        pcAt=self.pc; w=self.mem[self.pc]; self.pc+=1; i=decode(w)
        a=self.regs[i['rs']]; b=self.regs[i['rt']]; o=i['op']
        if o==1: self.w(i['rd'],i['imm'])
        elif o==2: self.w(i['rd'],self.mem[i['imm']]); self.hitMem=i['imm']
        elif o==3: self.mem[i['imm']]=self.regs[i['rd']]; self.hitMem=i['imm']
        elif o==4: self.w(i['rd'],a)
        elif o==5: r=a+b; self.sf(r); self.w(i['rd'],r)
        elif o==6: r=(a-b)&0x1FFFF; self.sf(r); self.w(i['rd'],r)
        elif o==7: self.w(i['rd'],a&b); self.sf(a&b)
        elif o==8: self.w(i['rd'],a|b); self.sf(a|b)
        elif o==9: self.w(i['rd'],a^b); self.sf(a^b)
        elif o==10: self.w(i['rd'],(~a)&0xFFFF); self.sf((~a)&0xFFFF)
        elif o==11: self.w(i['rd'],(a<<(b&15))&0xFFFF); self.sf(a<<(b&15))
        elif o==12: self.w(i['rd'],a>>(b&15)); self.sf(a>>(b&15))
        elif o==13: r=(a-b)&0x1FFFF; self.sf(r)
        elif o==14: self.pc=i['addr']
        elif o==15:
            if self.fz: self.pc=i['addr']
        elif o==16:
            if not self.fz: self.pc=i['addr']
        elif o==17: self.sp-=1; self.mem[self.sp]=self.regs[i['rd']]; self.hitMem=self.sp
        elif o==18: self.w(i['rd'],self.mem[self.sp]); self.hitMem=self.sp; self.sp+=1
        elif o==19: self.halted=True
        elif o==20: r=(a*b)&0x1FFFF; self.sf(r); self.w(i['rd'],r)
        elif o==21:
            if b==0: self.w(i['rd'],0); self.fc=True
            else: q=a//b; self.sf(q); self.w(i['rd'],q)
        elif o==22: self.sp-=1; self.mem[self.sp]=self.pc; self.pc=i['addr']
        elif o==23: self.pc=self.mem[self.sp]; self.sp+=1
        elif o==24: self.outputs.append(self.regs[i['rd']])
        self.cyc+=1
        return dict(pcAt=pcAt, op=o)

PROGRAMS = {
 "Fibonacci": """; first 8 Fibonacci numbers
LOADI R1, 0
LOADI R2, 1
LOADI R4, 8
LOADI R6, 1
loop: OUT R1
ADD R3, R1, R2
MOV R1, R2
MOV R2, R3
SUB R4, R4, R6
JNZ loop
HALT""",
 "GCD(48,36)": """; gcd via Euclid, CALL/RET
LOADI R1, 48
LOADI R2, 36
LOADI R7, 0
CALL gcd
OUT R1
HALT
gcd: CMP R2, R7
JZ done
DIV R3, R1, R2
MUL R4, R3, R2
SUB R5, R1, R4
MOV R1, R2
MOV R2, R5
JMP gcd
done: RET""",
 "Multiply 6x7": """; 6*7 by repeated addition
LOADI R1, 6
LOADI R2, 7
LOADI R3, 0
LOADI R6, 1
LOADI R7, 0
loop: CMP R2, R7
JZ done
ADD R3, R3, R1
SUB R2, R2, R6
JMP loop
done: OUT R3
HALT""",
 "Countdown": """LOADI R1, 5
LOADI R6, 1
LOADI R7, 0
loop: OUT R1
SUB R1, R1, R6
CMP R1, R7
JNZ loop
HALT""",
}

# palette
BG=(10,14,15); PANEL=(16,23,26); PANEL2=(12,18,20); LINE=(29,42,46)
INK=(207,232,228); DIM=(90,117,112); LABEL=(127,163,156)
PHOS=(62,240,196); PHOSD=(26,107,88); AMBER=(240,178,62); RED=(240,86,62)
GLOW=(62,240,196)

# gui
class Button:
    def __init__(self,rect,label,cb,accent=False):
        self.rect=pygame.Rect(rect); self.label=label; self.cb=cb
        self.accent=accent; self.hover=False
    def draw(self,surf,font):
        col=PANEL2 if self.hover else PANEL
        pygame.draw.rect(surf,col,self.rect,border_radius=4)
        pygame.draw.rect(surf,PHOS if (self.accent or self.hover) else LINE,self.rect,1,border_radius=4)
        c=PHOS if self.accent else INK
        t=font.render(self.label,True,c)
        surf.blit(t,(self.rect.centerx-t.get_width()//2,self.rect.centery-t.get_height()//2))
    def handle(self,ev):
        if ev.type==pygame.MOUSEMOTION:
            self.hover=self.rect.collidepoint(ev.pos)
        elif ev.type==pygame.MOUSEBUTTONDOWN and self.rect.collidepoint(ev.pos):
            self.cb()

class App:
    def __init__(self):
        pygame.init()
        self.W,self.H=1180,860
        self.screen=pygame.display.set_mode((self.W,self.H))
        pygame.display.set_caption("MIRA-16  ·  16-bit Pipelined CPU")
        self.clock=pygame.time.Clock()
        def mkfont(sz,bold=False):
            for name in ["Menlo","Consolas","DejaVu Sans Mono","Courier New","monospace"]:
                try:
                    f=pygame.font.SysFont(name,sz,bold=bold)
                    if f: return f
                except Exception: pass
            return pygame.font.Font(None,sz)
        self.f=mkfont(15); self.fb=mkfont(15,True); self.fs=mkfont(12)
        self.fbig=mkfont(22,True); self.ftiny=mkfont(11)
        self.cpu=CPU(); self.code=[]; self.errors=[]
        self.pipeline=[None]*5
        self.running_anim=False
        self.tick_ms=120; self.acc=0
        self.prog_names=list(PROGRAMS.keys()); self.prog_idx=0
        self.reg_glow=[0.0]*8   # fade timers for register highlights
        self.stage_glow=[0.0]*5
        self.var_slots={}       # var name -> mem address (from MiniC compile)
        self.mem_glow={}        # addr -> fade timer
        # MiniC editable source + edit state
        self.minic_text=("var n = 5;\n"
                         "var result = 1;\n"
                         "while (n > 0) {\n"
                         "  result = result * n;\n"
                         "  n = n - 1;\n"
                         "}\n"
                         "print result;")
        self.editing=False     # is the code box focused?
        self.cursor_on=True; self.cursor_acc=0
        self.buttons=[]
        self._build_buttons()
        self.load_program(0)

    def _build_buttons(self):
        self.buttons=[]
        x=18; y=58; w=84; h=30; gap=8
        defs=[("STEP",self.do_step,True),("RUN",self.do_run,False),
              ("PAUSE",self.do_pause,False),("RESET",self.do_reset,False)]
        for lab,cb,acc in defs:
            self.buttons.append(Button((x,y,w,h),lab,cb,acc)); x+=w+gap
        x+=20
        for i,name in enumerate(self.prog_names):
            self.buttons.append(Button((x,y,96,h),name,lambda i=i:self.load_program(i)))
            x+=96+gap
        # MiniC compile-and-run button, sits above the code editor
        self.compile_btn=Button((18,self.H-282,200,26),"▶ COMPILE & RUN MiniC",
                                self.compile_and_run_minic,True)
        self.buttons.append(self.compile_btn)

    def load_program(self,idx):
        self.do_pause(); self.prog_idx=idx
        src=PROGRAMS[self.prog_names[idx]]
        code,errs=assemble(src)
        self.errors=errs
        if errs: return
        self.code=code; self.cpu=CPU(); self.cpu.load(code)
        self.var_slots={}      # asm programs have no named variables
        self.mem_glow={}
        self.pipeline=[None]*5; self.reg_glow=[0.0]*8; self.stage_glow=[0.0]*5

    def compile_and_run_minic(self):
        self.do_pause()
        res=compile_minic(self.minic_text)
        if res[0] is None:
            self.errors=res[1]; return
        asm, _, slots = res
        code,aerr=assemble(asm)
        self.errors=aerr
        if aerr: return
        self.code=code; self.cpu=CPU(); self.cpu.load(code)
        self.var_slots=slots          # show these in the memory panel
        self.mem_glow={}
        self.pipeline=[None]*5; self.reg_glow=[0.0]*8; self.stage_glow=[0.0]*5
        self.do_run()                  # auto-run so you see it execute

    def tick_cpu(self):
        for i in range(4,0,-1): self.pipeline[i]=self.pipeline[i-1]
        self.pipeline[0]=None
        if not self.cpu.halted:
            pcAt=self.cpu.pc; in_=decode(self.cpu.mem[pcAt]); self.cpu.step()
            self.pipeline[0]=(OPNAME[in_['op']],pcAt)
            self.stage_glow[0]=1.0
            if self.cpu.hitReg>=0: self.reg_glow[self.cpu.hitReg]=1.0
            if self.cpu.hitMem>=0: self.mem_glow[self.cpu.hitMem]=1.0
        # shift stage glows with the pipeline
        for i in range(4,0,-1): self.stage_glow[i]=max(self.stage_glow[i],self.stage_glow[i-1]*0.7)

    def do_step(self):
        if not self.running_anim: self.tick_cpu()
    def do_run(self): self.running_anim=True
    def do_pause(self): self.running_anim=False
    def do_reset(self):
        self.do_pause(); self.cpu=CPU(); self.cpu.load(self.code)
        self.pipeline=[None]*5; self.reg_glow=[0.0]*8; self.stage_glow=[0.0]*5
        self.mem_glow={}

    def _drained(self): return all(p is None for p in self.pipeline)

    # ---- drawing helpers ----
    def text(self,s,x,y,col,font,center=False,right=False):
        t=font.render(s,True,col)
        if center: x-=t.get_width()//2
        if right:  x-=t.get_width()
        self.screen.blit(t,(x,y))
    def panel_box(self,x,y,w,h,border,fill=PANEL,glow=0.0):
        if glow>0:
            g=pygame.Surface((w+12,h+12),pygame.SRCALPHA)
            a=int(70*glow)
            pygame.draw.rect(g,(*PHOS,a),(0,0,w+12,h+12),border_radius=8)
            self.screen.blit(g,(x-6,y-6))
        pygame.draw.rect(self.screen,fill,(x,y,w,h),border_radius=4)
        pygame.draw.rect(self.screen,border,(x,y,w,h),1,border_radius=4)

    def draw(self):
        s=self.screen; s.fill(BG)
        # header
        self.text("MIRA·16",18,14,INK,self.fbig)
        self.text("16-BIT PIPELINED PROCESSOR — IF · ID · EX · MEM · WB",
                  self.W-18,22,DIM,self.fs,right=True)
        for b in self.buttons: b.draw(s,self.fs)

        # pipeline hero
        self.text("PIPELINE",18,104,DIM,self.fb)
        names=[("IF","FETCH"),("ID","DECODE"),("EX","EXECUTE"),("MEM","MEMORY"),("WB","WRITEBACK")]
        pw=(self.W-36-32)/5; px=18; py=124; ph=92
        for i,(sh,fu) in enumerate(names):
            occ=self.pipeline[i]; glow=self.stage_glow[i]
            self.panel_box(px,py,pw,ph,PHOS if occ else LINE,PANEL,glow if occ else 0)
            self.text(sh,px+pw/2,py+10,LABEL,self.fs,center=True)
            self.text(fu,px+pw/2,py+26,DIM,self.ftiny,center=True)
            if occ:
                self.text(occ[0],px+pw/2,py+50,PHOS,self.fb,center=True)
                self.text(f"PC {occ[1]}",px+pw/2,py+70,DIM,self.fs,center=True)
            else:
                self.text("·",px+pw/2,py+52,PHOSD,self.fb,center=True)
            px+=pw+8

        # registers (left)
        ry=246
        self.text("REGISTERS",18,ry,DIM,self.fb)
        for i in range(8):
            rx=18+(i%2)*210; yy=ry+24+(i//2)*32; g=self.reg_glow[i]
            fill=(14,31,27) if g>0.05 else PANEL
            self.panel_box(rx,yy,200,26,PHOS if g>0.05 else LINE,fill,g)
            self.text(f"R{i}",rx+10,yy+6,LABEL,self.fs)
            self.text(str(self.cpu.regs[i]),rx+190,yy+6,INK,self.f,right=True)

        # flags + meta
        fy=ry+24+4*32+12
        flags=[("ZERO",self.cpu.fz),("NEG",self.cpu.fn),("CARRY",self.cpu.fc)]
        for i,(nm,on) in enumerate(flags):
            fx=18+i*138
            self.panel_box(fx,fy,128,26,PHOS if on else LINE,PANEL)
            self.text(nm,fx+64,fy+6,PHOS if on else DIM,self.fs,center=True)
        self.text(f"PC {self.cpu.pc}      SP {self.cpu.sp}      CYC {self.cpu.cyc}",
                  18,fy+40,DIM,self.f)
        if self.cpu.halted: self.text("● HALTED",18,fy+64,PHOS,self.fb)

        # program (right column, upper)
        lx=470; ly=246
        self.text("PROGRAM",lx,ly,DIM,self.fb)
        cur=self.cpu.pc
        for a,w in enumerate(self.code[:22]):
            yy=ly+24+a*16; here=(a==cur)
            if here: pygame.draw.rect(s,(14,38,32),(lx-4,yy-2,360,16),border_radius=3)
            self.text(f"{a:>2}  {disasm(decode(w))}",lx,yy,INK if here else DIM,
                      self.fb if here else self.f)

        # memory panel (far right): variable slots + stack top
        mx=860; my=246
        self.text("MEMORY",mx,my,DIM,self.fb)
        # variable slots from the last MiniC compile (named), else slots 200+
        rows=[]
        if self.var_slots:
            for nm,addr in sorted(self.var_slots.items(), key=lambda kv:kv[1]):
                rows.append((addr, nm))
        else:
            for a in range(200,206): rows.append((a,None))
        yy=my+24
        for addr,nm in rows:
            g=self.mem_glow.get(addr,0.0)
            self.panel_box(mx,yy,290,26,AMBER if g>0.05 else LINE,
                           (31,26,12) if g>0.05 else PANEL, 0)
            label = f"[{addr}] {nm}" if nm else f"mem[{addr}]"
            self.text(label,mx+10,yy+6,LABEL,self.fs)
            self.text(str(self.cpu.mem[addr]),mx+280,yy+6,INK,self.f,right=True)
            yy+=30
        # stack top (a few words below SP)
        self.text("STACK (top)",mx,yy+6,DIM,self.fs); yy+=26
        for a in range(self.cpu.sp, min(self.cpu.sp+3,65536)):
            g=self.mem_glow.get(a,0.0)
            self.panel_box(mx,yy,290,24,AMBER if g>0.05 else LINE,
                           (31,26,12) if g>0.05 else PANEL,0)
            self.text(f"[{a}]",mx+10,yy+5,LABEL,self.fs)
            self.text(str(self.cpu.mem[a]),mx+280,yy+5,INK,self.f,right=True)
            yy+=28

        # ---- MiniC code editor (bottom-left) ----
        ey=self.H-250
        self.text("MiniC SOURCE — type code, then Compile & Run",18,ey,LABEL,self.fb)
        box_y=ey+22; box_h=150; box_w=620
        focus = self.editing
        self.panel_box(18,box_y,box_w,box_h, PHOS if focus else LINE, PANEL2,0)
        self._code_box_rect=(18,box_y,box_w,box_h)
        # render the editable text, line by line
        lines=self.minic_text.split('\n')
        tx=28; tline_y=box_y+10
        for li,ln in enumerate(lines[:9]):
            self.text(ln, tx, tline_y+li*16, INK, self.fs)
        # blinking cursor at end of last line
        if focus and self.cursor_on:
            last=lines[-1] if lines else ""
            cw=self.fs.render(last,True,INK).get_width()
            cy=tline_y+(min(len(lines),9)-1)*16
            pygame.draw.line(s,PHOS,(tx+cw+1,cy),(tx+cw+1,cy+13),1)

        # output console (bottom, full width under the box)
        oy=self.H-66
        self.text("OUTPUT",660,ey,DIM,self.fb)
        outs="    ".join(str(v) for v in self.cpu.outputs) or "— no output yet —"
        self.text(outs,660,ey+24,AMBER,self.fbig)

        # error line
        if self.errors:
            self.text("  ".join(self.errors)[:90],660,ey+70,RED,self.fs)

        pygame.display.flip()

    def update(self,dt):
        # fade glows
        for i in range(8): self.reg_glow[i]=max(0,self.reg_glow[i]-dt/450.0)
        for i in range(5): self.stage_glow[i]=max(0,self.stage_glow[i]-dt/450.0)
        for a in list(self.mem_glow):
            self.mem_glow[a]=max(0,self.mem_glow[a]-dt/450.0)
            if self.mem_glow[a]<=0: del self.mem_glow[a]
        # cursor blink
        self.cursor_acc+=dt
        if self.cursor_acc>=500: self.cursor_acc=0; self.cursor_on=not self.cursor_on
        if self.running_anim:
            self.acc+=dt
            if self.acc>=self.tick_ms:
                self.acc=0
                if self.cpu.halted and self._drained():
                    self.running_anim=False
                else:
                    self.tick_cpu()

    def run(self):
        while True:
            dt=self.clock.tick(60)
            for ev in pygame.event.get():
                if ev.type==pygame.QUIT:
                    pygame.quit(); sys.exit()
                elif ev.type==pygame.MOUSEBUTTONDOWN:
                    # click inside code box -> focus it; click outside -> blur
                    bx,by,bw,bh=getattr(self,'_code_box_rect',(0,0,0,0))
                    self.editing = (bx<=ev.pos[0]<=bx+bw and by<=ev.pos[1]<=by+bh)
                    for b in self.buttons: b.handle(ev)
                elif ev.type==pygame.MOUSEMOTION:
                    for b in self.buttons: b.handle(ev)
                elif ev.type==pygame.KEYDOWN:
                    if self.editing:
                        # typing into the MiniC box
                        if ev.key==pygame.K_BACKSPACE:
                            self.minic_text=self.minic_text[:-1]
                        elif ev.key==pygame.K_RETURN:
                            self.minic_text+="\n"
                        elif ev.key==pygame.K_TAB:
                            self.minic_text+="  "
                        elif ev.unicode and ev.unicode.isprintable():
                            self.minic_text+=ev.unicode
                    else:
                        # global shortcuts (only when not editing)
                        if ev.key==pygame.K_SPACE: self.do_step()
                        elif ev.key==pygame.K_RETURN: self.do_run()
                        elif ev.key==pygame.K_r: self.do_reset()
            self.update(dt)
            self.draw()

if __name__=="__main__":
    App().run()
