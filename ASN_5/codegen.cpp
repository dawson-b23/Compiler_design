/**
 * @file codegen.cpp
 * @author Dawson Burgess (dawsonhburgess@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-05-20
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "treeNodes.h"
#include "treeUtils.h"
#include "symbolTable.h"
#include "emitcode.h"
#include "codegen.h"
#include "semantics.h"
#include "parser.tab.h" // always put last 

extern int numErrors;
extern int numWarnings;
extern int yyparse();
extern int yydebug;
extern TreeNode *syntaxTree;
extern char **largerTokens;
extern void initTokenStrings();

// these offsets never change 
#define OFPOFF 0
#define RETURNOFFSET -1

int toffset; // next available temporary space

FILE *code; // shared global code
static bool linenumFlag; // mark with line numbers
static int breakloc; // which while to break to
static SymbolTable *globals; // the global symbol table

// ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** 
// START

//// FUNCTION BEING CALLED IN MAIN (PARSER.Y) / MAIN OPERATION 

// this is the top level code generator call
void codegen(FILE *codeIn, // where the code should be written
            char *srcFile, // name of file compiled
            TreeNode *syntaxTree, // tree to process
            SymbolTable *globalsIn, // globals so function info can be found
            int globalOffset,
            bool linenumFlagIn) {

    int initJump;
    code = codeIn;
    globals = globalsIn;
    linenumFlag = linenumFlagIn;
    breakloc = 0;

    initJump = emitSkip(1); // save a place for the jump to init
    codegenHeader(srcFile); // nice comments describing what is compiled
    codegenGeneral(syntaxTree); // general code generation including I/O library
    codegenInit(initJump, globalOffset); // generation of initialization for run
}

// END 
// ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** 



// ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** 
// START

//// HELPER METHODS FOR CODEGEN

// debugger
void debugPrint(const char *input) {
    if(1 == 0) {
        printf("%s\n", input);
    }
}

// Generate a header for our code
void codegenHeader(char *srcFile) {
    emitComment((char *)"bC compiler version bC-Su23");
    emitComment((char *)"File compiled: ", srcFile);
}

void commentLineNum(TreeNode *currnode) {
    char buf[16];
    if (linenumFlag) {
        sprintf(buf, "%d", currnode->lineno);
        emitComment((char *)"Line: ", buf);
    }
}

void initGlobalArraySizes() {
    emitComment((char *)"INIT GLOBALS AND STATICS");
    globals->applyToAllGlobal(initAGlobalSymbol);
    emitComment((char *)"END INIT GLOBALS AND STATICS");
}

void initAGlobalSymbol(std::string sym, void *ptr) {
    TreeNode *currnode;

    // printf("Symbol: %s\n", sym.c_str()); // dump the symbol table
    currnode = (TreeNode *)ptr;
    // printf("lineno: %d\n", currnode->lineno); // dump the symbol table
    
    if (currnode->lineno != -1) {
        if (currnode->isArray) {
            emitRM((char *)"LDC", AC, currnode->size-1, 6, (char *)"load size of array", currnode->attr.name);
            emitRM((char *)"ST", AC, currnode->offset+1, GP, (char *)"save size of array", currnode->attr.name);
        }
        if (currnode->kind.decl==VarK && (currnode->varKind == Global || currnode->varKind == LocalStatic)) {
            if (currnode->child[0]) { 
                // compute rhs -> AC;
                codegenExpression(currnode->child[0]);
            
                // save it
                emitRM((char *)"ST", AC, currnode->offset, GP,
                (char *)"Store variable", currnode->attr.name);
            }
        }
    }
}

int offsetRegister(VarKind v) {
    debugPrint("Offset Register");
    switch (v) {
        case Local: 
            return FP;
    
        case Parameter: 
            return FP;
    
        case Global: 
            return GP;
    
        case LocalStatic: 
            return GP;
   
    default:
        printf((char *)"ERROR(codegen): looking up offset register for a variable of type %d\n", v);
        return 666;
    }
}

// END 
// ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** 



// ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** 
// START

//// INIT AND GENERAL

// Generate init code ...
void codegenInit(int initJump, int globalOffset) {
    backPatchAJumpToHere(initJump, (char *)"Jump to init [backpatch]");

    emitComment((char *)"INIT"); 
    //OLD pre 4.6 TM emitRM((char *)"LD", GP, 0, 0, (char *)"Set the global pointer");
    emitRM((char *)"LDA", FP, globalOffset, GP, (char *)"set first frame at end of globals");
    emitRM((char *)"ST", FP, 0, FP, (char *)"store old fp (point to self)");

    initGlobalArraySizes();

    emitRM((char *)"LDA", AC, 1, PC, (char *)"Return address in ac");
    { // jump to main
        TreeNode *funcNode;
        funcNode = (TreeNode *)(globals->lookup((char *)"main"));
        if (funcNode) { 
            emitGotoAbs(funcNode->offset, (char *)"Jump to main");
        } 
        else { 
            printf((char *)"ERROR(LINKER): Procedure main is not defined.\n");
            numErrors++;
        }
    }

    emitRO((char *)"HALT", 0, 0, 0, (char *)"DONE!");
    emitComment((char *)"END INIT");
}

void codegenGeneral(TreeNode *currnode) { 
    debugPrint("CodeGen General");
    while (currnode) {
        switch (currnode->nodekind) {
            case StmtK:
                codegenStatement(currnode);
                break;
            case ExpK:
                emitComment((char *)"EXPRESSION"); 
                codegenExpression(currnode);
                break;
            case DeclK:
                codegenDecl(currnode);
                break;
        }
        currnode = currnode->sibling;
    }
}

// END 
// ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** 



// ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** 
// START

//// FUNCTION PROCESSING

void codegenLibraryFun(TreeNode *currnode) {
    emitComment((char *)"");
    emitComment((char *)"** ** ** ** ** ** ** ** ** ** ** **");
    emitComment((char *)"FUNCTION", currnode->attr.name);

    // remember where this function is
    currnode->offset = emitSkip(0);

    // Store return address
    emitRM((char *)"ST", AC, RETURNOFFSET, FP, (char *)"Store return address");

    // Next slides here
    if (strcmp(currnode->attr.name, (char *)"input")==0) {
        emitRO((char *)"IN", RT, RT, RT, (char *)"Grab int input");
    } 
    else if (strcmp(currnode->attr.name, (char *)"inputb")==0) {
        emitRO((char *)"INB", RT, RT, RT, (char *)"Grab bool input");
    } 
    else if (strcmp(currnode->attr.name, (char *)"inputc")==0) {
        emitRO((char *)"INC", RT, RT, RT, (char *)"Grab char input");
    } 
    else if (strcmp(currnode->attr.name, (char *)"output")==0) {
        emitRM((char *)"LD", AC, -2, FP, (char *)"Load parameter");
        emitRO((char *)"OUT", AC, AC, AC, (char *)"Output integer");
    } 
    else if (strcmp(currnode->attr.name, (char *)"outputb")==0) {
        emitRM((char *)"LD", AC, -2, FP, (char *)"Load parameter");
        emitRO((char *)"OUTB", AC, AC, AC, (char *)"Output bool");
    } 
    else if (strcmp(currnode->attr.name, (char *)"outputc")==0) {
        emitRM((char *)"LD", AC, -2, FP, (char *)"Load parameter");
        emitRO((char *)"OUTC", AC, AC, AC, (char *)"Output char");
    } 
    else if (strcmp(currnode->attr.name, (char *)"outnl")==0) {
        emitRO((char *)"OUTNL", AC, AC, AC, (char *)"Output a newline");
    } 
    else {
        emitComment((char *)"ERROR(LINKER): No support for special function");
        emitComment(currnode->attr.name);
    }

    emitRM((char *)"LD", AC, RETURNOFFSET, FP, (char *)"Load return address");
    emitRM((char *)"LD", FP, OFPOFF, FP, (char *)"Adjust fp");
    emitGoto(0, AC, (char *)"Return");

    emitComment((char *)"END FUNCTION", currnode->attr.name);
}

// process functions
void codegenFun(TreeNode *currnode) {
    emitComment((char *)"");
    emitComment((char *)"** ** ** ** ** ** ** ** ** ** ** **");
    emitComment((char *)"FUNCTION", currnode->attr.name);
    toffset = currnode->size; // recover the end of activation record
    emitComment((char *)"TOFF set:", toffset);

    // function in the code space! This is accessible via the symbol table. // remember where this function is:
    currnode->offset = emitSkip(0); // offset holds the instruction address!!

    // Store return address
    emitRM((char *)"ST", AC, RETURNOFFSET, FP, (char *)"Store return address");

    // Generate code for the statements...
    codegenGeneral(currnode->child[1]);

    // In case there was no return statement 
    // set return register to 0 and return
    emitComment((char *)"Add standard closing in case there is no return statement");
    emitRM((char *)"LDC", RT, 0, 6, (char *)"Set return value to 0");
    emitRM((char *)"LD", AC, RETURNOFFSET, FP, (char *)"Load return address");
    emitRM((char *)"LD", FP, OFPOFF, FP, (char *)"Adjust fp");
    emitGoto(0, AC, (char *)"Return");

    emitComment((char *)"END FUNCTION", currnode->attr.name);
}

// END 
// ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** 



// ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** 
// START

//// BIG 3 METHODS 


void codegenDecl(TreeNode *currnode) {
    debugPrint("Codegen Decl");
    commentLineNum(currnode);
    switch(currnode->kind.decl) {
        case VarK:
            debugPrint("VarK");
            // You have a LOT to do here!!!!!
            if(currnode->isArray) {
                switch(currnode->varKind) {
                    case Local:
                        emitRM((char *)"LDC", AC, currnode->size-1, 6, (char *)"load size of array", currnode->attr.name);
                        emitRM((char *)"ST", AC, currnode->offset+1, offsetRegister(currnode->varKind), (char *)"save size of array", currnode->attr.name);
                        break;

                    case LocalStatic:
                        // fill this
                        break;

                    case Parameter:
                        // fill this 
                        break;

                    case Global:
                        // do nothing here 
                        break;

                    case None:
                        // Error Condition
                        break;
                } 
                // ARRAY VALUE initialization
                if(currnode->child[0]) { 
                    codegenExpression(currnode->child[0]);
                    emitRM((char *)"LDA", AC1, currnode->offset, offsetRegister(currnode->varKind), (char *)"address of lhs");
                    emitRM((char *)"LD", AC2, 1, AC, (char *)"size of rhs");
                    emitRM((char *)"LD", AC3, 1, AC1, (char *)"size of lhs");
                    emitRO((char *)"SWP", AC2, AC3, 6, (char *)"pick smallest size");
                    emitRO((char *)"MOV", AC1, AC, AC2, (char *)"array op =");
                }
            }
            else { 
                // !currnode->isArray 
                // SCALAR VALUE initialization
                if(currnode->child[0]) {
                    switch(currnode->varKind) {
                        case Local:
                            // compute rhs -> AC;
                            codegenExpression(currnode->child[0]);
        
                            // save it
                            emitRM((char *)"ST", AC, currnode->offset, FP, (char *)"Store variable", currnode->attr.name);
                            break;
                            
                        case LocalStatic:
                            break;
                        case Parameter: 
                            break;
                        case Global:
                            // do nothing here break;
                            break;
                        case None:
                            ///Error condition!!!
                            break;
                    }
                }
            }
            break;

        case FuncK:
            debugPrint("FuncK");
            if (currnode->lineno == -1) { // These are the library functions we just added
                codegenLibraryFun(currnode);
            } 
            else {
                codegenFun(currnode);
            } 
            break;

        case ParamK:
            // IMPORTANT: no instructions need to be allocated for parameters here
            break;
    }
}

void codegenStatement(TreeNode *currnode) { 
    debugPrint("Codegen Statement");
    // local state to remember stuff
    int skiploc = 0, skiploc2 = 0, currloc = 0; // some temporary instuction addresses
    TreeNode *loopindex = NULL; // a pointer to the index variable declaration node
    commentLineNum(currnode);

    switch(currnode->kind.stmt) {
        case IfK:
            debugPrint("IfK");
            emitComment((char *)"IF");
            currloc = emitSkip(0); 
            codegenExpression(currnode->child[0]); // test expression
            skiploc = emitSkip(1);


            emitComment((char *)"THEN");
            codegenGeneral(currnode->child[1]); // do the if body 
            if(currnode->child[2]){
                skiploc2 = emitSkip(1); ///////////////////////////////BC WE need a second skip loc for the then
            }

            backPatchAJumpToHere((char *)"JZR", AC, skiploc, (char *)"Jump around the THEN if false [backpatch]"); // backpatch jump to end of loop
            if(currnode->child[2]) {
                emitComment((char *)"ELSE");
                codegenGeneral(currnode->child[2]); // do the else
                backPatchAJumpToHere(skiploc2, (char *)"Jump around the ELSE [backpatch]");//JMP  7,0(7)  Jump around the ELSE [backpatch] 
            }
            
            emitComment((char *)"END IF");
            break;

        case WhileK:
            debugPrint("WhileK");
            emitComment((char *)"WHILE");
            currloc = emitSkip(0); // return to here to do the test

            codegenExpression(currnode->child[0]); // test expression
                        
            emitRM((char *)"JNZ", AC, 1, PC, (char *)"Jump to while part");
            emitComment((char *)"DO");
            
            skiploc = breakloc;     // save the old break statement return point
            breakloc = emitSkip(1); // addr of instr that jumps to end of loop 
                                    // this is also the backpatch point
            
            if(currnode->child[1]) {
                codegenGeneral(currnode->child[1]); // do body of loop
            }
            emitGotoAbs(currloc, (char *)"go to beginning of loop");
            backPatchAJumpToHere(breakloc, (char *)"Jump past loop [backpatch]"); // backpatch jump to end of loop

            breakloc = skiploc; // restore for break statement
            emitComment((char *)"END WHILE");

            break;

        case ForK:
            debugPrint("ForK");
            int savedToffset;
            int startoff, stopoff, stepoff;
            savedToffset = toffset;
            toffset = currnode->size;

            emitComment((char *)"TOFF set:", toffset);
            emitComment((char *)"FOR");
            loopindex = currnode->child[0];
            startoff = loopindex->offset;
            stopoff = startoff-1;
            stepoff = startoff-2;

            TreeNode *rangenode, *tmp;
            rangenode = currnode->child[1];
            if(rangenode->child[0]) {
                tmp = rangenode->child[0];
                codegenExpression(tmp);
                emitRM((char *)"ST", AC, startoff, FP, (char *)"save starting value in index variable");
            }
            if(rangenode->child[1]) {
                tmp = rangenode->child[1];
                codegenExpression(tmp);
            }

            if(rangenode->child[1] == NULL) {
                codegenExpression(tmp);
                
            }
            emitRM((char *)"ST", AC, stopoff, FP, (char *)"save stop value");

            if(rangenode->child[2]!=NULL) {
                tmp = rangenode->child[2];
                codegenExpression(tmp);
            }
            if(rangenode->child[2] == NULL) {
                emitRM((char *)"LDC", AC, 1, 6, (char *)"default increment by 1"); //LDC  3,2(6)   Load integer constant
                
            }
            emitRM((char *)"ST", AC, stepoff, FP, (char *)"save step value");

            currloc = emitSkip(0); // return to here to do the test
            emitRM((char *)"LD", AC1, startoff, FP, (char *)"loop index");
            emitRM((char *)"LD", AC2, stopoff, FP, (char *)"stop value");
            emitRM((char *)"LD", AC, stepoff, FP, (char *)"step value");
            emitRO((char *)"SLT", AC, AC1, AC2, (char *)"Op <");
            emitRM((char *)"JNZ", AC, 1, PC, (char *)"Jump to loop body");

            skiploc = breakloc; 
            breakloc = emitSkip(1);
            codegenGeneral(currnode->child[2]); // do body of loop
            emitComment((char *)"Bottom of loop increment and jump");
            emitRM((char *)"LD", AC, startoff, FP, (char *)"Load index");
            emitRM((char *)"LD", AC2, stepoff, FP, (char *)"Load step");
            emitRO((char *)"ADD", AC, AC, AC2, (char *)"increment");
            emitRM((char *)"ST", AC, startoff, FP, (char *)"store back to index");
            

            emitGotoAbs(currloc, (char *)"go to beginning of loop");
            backPatchAJumpToHere(breakloc, (char *)"Jump past loop [backpatch]"); // backpatch jump to end of loop
            breakloc = skiploc;
            

            emitComment((char *)"END LOOP");
            
            break;

        case CompoundK: {
            debugPrint("CompoundK");
            int savedToffset;

            savedToffset = toffset;
            toffset = currnode->size; // recover the end of activation record
            emitComment((char *)"COMPOUND");
            emitComment((char *)"TOFF set:", toffset);
            codegenGeneral(currnode->child[0]); // process inits
            emitComment((char *)"Compound Body"); codegenGeneral(currnode->child[1]); // process body
            toffset = savedToffset;
            emitComment((char *)"TOFF set:", toffset);
            emitComment((char *)"END COMPOUND");
        }
        break;
        
        case ReturnK:
            debugPrint("ReturnK");
            emitComment((char *)"RETURN");
            if(currnode->child[0]) {
                codegenExpression(currnode->child[0]);
                emitRM((char *)"LDA", RT, currnode->offset, AC, (char *)"Copy result to return register");
            }
            emitRM((char *)"LD", AC, RETURNOFFSET, FP,(char *)"Load return address");
            emitRM((char *)"LD", FP, OFPOFF, FP,(char *)"Adjust fp");
            emitGoto(0, AC, (char *)"Return");
            break;
            break;

        case BreakK:
            debugPrint("BreakK");
            emitComment((char *)"BREAK");
            emitGotoAbs(breakloc, (char *)"break");
            break;
        
        case RangeK:
            debugPrint("RangeK");
            break;

        default:
            break;
        }
}

void codegenExpression(TreeNode *currnode) {
    debugPrint("Codegen Expression");
    TreeNode *rhs, *lhs, *var, *index;
    lhs = currnode->child[0]; 
    rhs = currnode->child[1];
    commentLineNum(currnode);
    int callLoc = 0;
    switch(currnode->kind.exp) {
        case AssignK:
            debugPrint("AssignK");
            if (lhs->attr.op == '[') { // array is true
                debugPrint("lhs->attr.op == [");
                var = lhs->child[0];
                index = lhs->child[1];

                debugPrint("AssignK, lhs->attr.op == [, codegenExp");
                codegenExpression(index);
                if(rhs) {
                    emitRM((char *)"ST", AC, toffset, FP, (char *)"Push index");
                    toffset--; 
                    emitComment((char *)"TOFF dec:", toffset);
                    codegenExpression(rhs);
                    toffset++; 
                    emitComment((char *)"TOFF inc:", toffset);
                    emitRM((char *)"LD", AC1, toffset, FP, (char *)"Pop index");
                }
                if(var->varKind == Global) {
                    emitRM((char *)"LDA", AC2, var->offset, GP, (char *)"Load address of base of array", var->attr.name);
                }
                if(var->varKind == Local) {
                    emitRM((char *)"LDA", AC2, var->offset, FP, (char *)"Load address of base of array", var->attr.name);
                }
                if(var->varKind == LocalStatic) {
                    emitRM((char *)"LDA", AC2, var->offset, GP, (char *)"Load address of base of array", var->attr.name);
                }
                if(var->varKind == Parameter) {
                    emitRM((char *)"LD", AC2, var->offset, FP, (char *)"Load address of base of array", var->attr.name);
                }
                if(rhs) {
                    emitRO((char *)"SUB", AC2, AC2, AC1, (char *)"Compute offset of value");
                }
                if(!rhs) {
                    emitRO((char *)"SUB", AC2, AC2, AC, (char *)"Compute offset of value");
                }
                switch(currnode->attr.op) {
                    case INC:
                        emitRM((char *)"LD", AC, lhs->offset, AC2, (char *)"load lhs variable", var->attr.name);
                        emitRM((char *)"LDA", AC, 1, AC, (char *)"increment value of", var->attr.name);
                        emitRM((char *)"ST", AC, lhs->offset, AC2, (char *)"Store variable", var->attr.name);
                        break;

                    case DEC:
                        emitRM((char *)"LD", AC, lhs->offset, AC2, (char *)"load lhs variable", var->attr.name);
                        emitRM((char *)"LDA", AC, -1, AC, (char *)"decrement value of", var->attr.name);
                        emitRM((char *)"ST", AC, lhs->offset, AC2, (char *)"Store variable", var->attr.name);
                        break;

                    case ADDASS:
                        emitRM((char *)"LD", AC1, lhs->offset, AC2,(char *)"load lhs variable", var->attr.name);
                        emitRO((char *)"ADD", AC, AC1, AC, (char *)"op +=");
                        emitRM((char *)"ST", AC, lhs->offset, AC2, (char *)"Store variable", var->attr.name);
                        break;

                    case DIVASS:
                        emitRM((char *)"LD", AC1, lhs->offset, AC2,(char *)"load lhs variable", var->attr.name);
                        emitRO((char *)"DIV", AC, AC1, AC, (char *)"op /=");
                        emitRM((char *)"ST", AC, lhs->offset, AC2, (char *)"Store variable", var->attr.name);
                        break;

                    case MULASS:
                        emitRM((char *)"LD", AC1, lhs->offset, AC2,(char *)"load lhs variable", var->attr.name);
                        emitRO((char *)"MUL", AC, AC1, AC, (char *)"op *=");
                        emitRM((char *)"ST", AC, lhs->offset, AC2, (char *)"Store variable", var->attr.name);
                        break;

                    case SUBASS:
                        emitRM((char *)"LD", AC1, lhs->offset, AC2,(char *)"load lhs variable", var->attr.name);
                        emitRO((char *)"SUB", AC, AC1, AC, (char *)"op -=");
                        emitRM((char *)"ST", AC, lhs->offset, AC2, (char *)"Store variable", var->attr.name);
                        break;

                    default:
                        emitRM((char *)"ST", AC, lhs->offset, AC2, (char *)"Store variable", var->attr.name);
                }


            } 
            else { //array is false 
                int offReg;
                offReg = offsetRegister(lhs->varKind); 
                debugPrint("AssignK else");
                switch(currnode->attr.op) {
                    case ADDASS:
                        codegenExpression(rhs);
                        emitRM((char *)"LD", AC1, lhs->offset, offReg,(char *)"load lhs variable", lhs->attr.name);
                        emitRO((char *)"ADD", AC, AC1, AC, (char *)"op +=");
                        emitRM((char *)"ST", AC, lhs->offset, offReg, (char *)"Store variable", lhs->attr.name);
                        break;
                    
                    case SUBASS:
                        codegenExpression(rhs);
                        emitRM((char *)"LD", AC1, lhs->offset, offReg,(char *)"load lhs variable", lhs->attr.name);
                        emitRO((char *)"SUB", AC, AC1, AC, (char *)"op -=");
                        emitRM((char *)"ST", AC, lhs->offset, offReg, (char *)"Store variable", lhs->attr.name);
                        break;

                    case DIVASS:
                        codegenExpression(rhs);
                        emitRM((char *)"LD", AC1, lhs->offset, offReg,(char *)"load lhs variable", lhs->attr.name);
                        emitRO((char *)"DIV", AC, AC1, AC, (char *)"op /=");
                        emitRM((char *)"ST", AC, lhs->offset, offReg, (char *)"Store variable", lhs->attr.name);
                        break;

                    case MULASS:
                        codegenExpression(rhs);
                        emitRM((char *)"LD", AC1, lhs->offset, offReg,(char *)"load lhs variable", lhs->attr.name);
                        emitRO((char *)"MUL", AC, AC1, AC, (char *)"op *=");
                        emitRM((char *)"ST", AC, lhs->offset, offReg, (char *)"Store variable", lhs->attr.name);
                        break;

                    case 286: // DEC -- 
                        emitRM((char *)"LD", AC, lhs->offset, offReg,(char *)"load lhs variable", lhs->attr.name);
                        emitRM((char *)"LDA", AC, -1, AC, (char *)"decrement value of", lhs->attr.name);
                        emitRM((char *)"ST", AC, lhs->offset, offReg, (char *)"Store variable", lhs->attr.name);
                        break;

                    case 287: // INC ++
                        emitRM((char *)"LD", AC, lhs->offset, offReg,(char *)"load lhs variable", lhs->attr.name);
                        emitRM((char *)"LDA", AC, 1, AC, (char *)"increment value of", lhs->attr.name);
                        emitRM((char *)"ST", AC, lhs->offset, offReg, (char *)"Store variable", lhs->attr.name);
                        break;

                    case '=':
                        codegenExpression(rhs);
                        emitRM((char *)"ST", AC, lhs->offset, offReg, (char *)"Store variable", lhs->attr.name);
                        break;
                }
        }
            break;

        case CallK: {
            debugPrint("CallK");
            int savedToffset;
            TreeNode *funcNode = ((TreeNode *)(globals->lookup(currnode->attr.name)));
            callLoc = funcNode->offset;
            savedToffset = toffset;

            emitComment((char *)"CALL", currnode->attr.name);
            emitRM((char *)"ST", FP, toffset, FP, (char *)"Store fp in ghost frame for", currnode->attr.name);
            toffset--; 
            emitComment((char *)"TOFF dec:", toffset);
            toffset--; 
            emitComment((char *)"TOFF dec:", toffset);
        
            TreeNode *param = currnode->child[0];
            int paramCount = 1;
            while(param) {
                emitComment((char *)"Param", paramCount);
                codegenExpression(param);
                emitRM((char *)"ST", AC, toffset, FP, (char *)"Push parameter");
                toffset--; 
                emitComment((char *)"TOFF dec:", toffset);
                paramCount++;
                param = param->sibling;
            }
            emitComment((char *)"Param end", currnode->attr.name);
            emitRM((char *)"LDA", FP, savedToffset, FP, (char *)"Ghost frame becomes new active frame");
            emitRM((char *)"LDA", AC, FP, PC, (char *)"Return address in ac");
            emitRMAbs((char *)"JMP", PC, callLoc, (char *)"CALL", currnode->attr.name);
            emitRM((char *)"LDA", AC, currnode->offset, RT, (char *)"Save the result in ac");
            emitComment((char *)"Call end", currnode->attr.name);
            toffset = savedToffset;
            emitComment((char *)"TOFF set:", toffset);
            break;
        }

        case ConstantK:
            debugPrint("ConstantK");
            switch(currnode->type) {
                case Boolean:
                    if(currnode->isArray == false) {
                        emitRM((char *)"LDC", AC, currnode->attr.value, 6, (char *)"Load Boolean constant");
                    }
                    break;
                
                case Integer:
                    emitRM((char *)"LDC", AC, currnode->attr.value, 6, (char *)"Load integer constant");
                    break;

                case Char:  
                    if(currnode->isArray) {
                        emitStrLit(currnode->offset, currnode->attr.string);
                        emitRM((char *)"LDA", AC, currnode->offset, 0, (char *)"Load char constant");
                    }
                    else {
                        emitRM((char *)"LDC", AC, int(currnode->attr.cvalue), 6, (char *)"Load char constant");
                    }
                    break;
                 
            }
            break;

        case IdK:
            debugPrint("IdK");
            int off;
            off = offsetRegister(currnode->varKind); 
            if(currnode->isArray == false) {
                if(lhs == NULL) {
                    emitRM((char *)"LD", AC, currnode->offset, off, (char *)"Load variable", currnode->attr.name);
                }
            }
            if(currnode->isArray == true) {
                if(currnode->varKind != Parameter) {
                    emitRM((char *)"LDA", AC, currnode->offset, off, (char *)"Load address of base of array", currnode->attr.name);
                }
                else {
                    emitRM((char *)"LD", AC, currnode->offset, off, (char *)"Load address of base of array", currnode->attr.name);
                }
            }
            break;

        case OpK:
            debugPrint("OpK");
            codegenExpression(lhs);
            if(rhs) {
                emitRM((char *)"ST", AC, toffset, FP, (char *)"Push left side");
                toffset--; 
                emitComment((char *)"TOFF dec:", toffset);
                codegenExpression(rhs);
                toffset++; 
                emitComment((char *)"TOFF inc:", toffset);
                emitRM((char *)"LD", AC1, toffset, FP, (char *)"Pop left into ac1");
                switch(currnode->attr.op) {
                    case '+':
                        emitRO((char *)"ADD", AC, AC1, AC, (char *)"Op +");
                        break;

                    case 284: // LEQ <=
                        emitRO((char *)"TLE", AC, AC1, AC, (char *)"Op <=");
                        break;

                    case '<':
                        emitRO((char *)"TLT", AC, AC1, AC, (char *)"Op <");
                        break;

                    case '>':
                        emitRO((char *)"TGT", AC, AC1, AC, (char *)"Op >");
                        break;

                    case 283: // GEQ >=
                        emitRO((char *)"TGE", AC, AC1, AC, (char *)"Op >=");
                        break;

                    case 282: // EQ == 
                        emitRO((char *)"TEQ", AC, AC1, AC, (char *)"Op ==");
                        break;

                    case 285: // NEQ !=
                        emitRO((char *)"TNE", AC, AC1, AC, (char *)"Op !=");
                        break;

                    case '-':
                        emitRO((char *)"SUB", AC, AC1, AC, (char *)"Op -");
                        break;

                    case '*':
                        emitRO((char *)"MUL", AC, AC1, AC, (char *)"Op *");
                        break;

                    case '%':
                        emitRO((char *)"MOD", AC, AC1, AC, (char *)"Op %");
                        break;

                    case '/':
                        emitRO((char *)"DIV", AC, AC1, AC, (char *)"Op /");
                        break;

                    case 292: // MAX :>:
                        emitRO((char *)"SWP", AC1, AC, AC, (char *)"Op :>:");
                        break;

                    case 293: // MIN :<:
                        emitRO((char *)"SWP", AC, AC1, AC, (char *)"Op :<:");
                        break;

                    case 278:
                        emitRO((char *)"AND", AC, AC1, AC, (char *)"Op AND");
                        break;

                    case 273:
                        emitRO((char *)"OR", AC, AC1, AC, (char *)"Op OR");
                        break;

                    case '[':
                        emitRO((char *)"SUB", AC, AC1, AC, (char *)"compute location from index");
                        emitRM((char *)"LD", AC, lhs->attr.value, AC, (char *)"Load array element");
                        break;
                }
            }
            if(lhs) {
                switch(currnode->attr.op){
                    case '?':
                        emitRO((char *)"RND", AC, AC, AC3, (char *)"Op ?"); //RND  3,3,6      Op ?
                        break;

                    case 269: //SIZEOFF
                        emitRM((char *)"LD", AC, FP, AC, (char *)"Load array size");
                        break;

                    case 270:
                        emitRO((char *)"NEG", AC, AC, AC, (char *)"Op unary -");
                        break;

                    case 280: //NOT
                        // Check if lhs is false (0)
                        if(lhs->attr.value == 0) {
                            // Load 1 into AC1 (since NOT false = true)
                            emitRM((char *)"LDC", AC1, 1, 6, (char *)"Load 1");
                        } 
                        if(lhs->attr.value == 1) {
                            emitRM((char *)"LDC", AC1, 1, 6, (char *)"Load 1");
                        }
                        emitRO((char *)"XOR", AC, AC, AC1, (char *)"Op XOR to get logical not");
                        break;
                }
            }
            break;
        }
}

// END 
// ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** 