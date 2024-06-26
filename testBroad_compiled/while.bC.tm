* bC compiler version bC-Su23
* File compiled:  while.bC
* 
* ** ** ** ** ** ** ** ** ** ** ** **
* FUNCTION input
  1:     ST  3,-1(1)	Store return address 
  2:     IN  2,2,2	Grab int input 
  3:     LD  3,-1(1)	Load return address 
  4:     LD  1,0(1)	Adjust fp 
  5:    JMP  7,0(3)	Return 
* END FUNCTION input
* 
* ** ** ** ** ** ** ** ** ** ** ** **
* FUNCTION output
  6:     ST  3,-1(1)	Store return address 
  7:     LD  3,-2(1)	Load parameter 
  8:    OUT  3,3,3	Output integer 
  9:     LD  3,-1(1)	Load return address 
 10:     LD  1,0(1)	Adjust fp 
 11:    JMP  7,0(3)	Return 
* END FUNCTION output
* 
* ** ** ** ** ** ** ** ** ** ** ** **
* FUNCTION inputb
 12:     ST  3,-1(1)	Store return address 
 13:    INB  2,2,2	Grab bool input 
 14:     LD  3,-1(1)	Load return address 
 15:     LD  1,0(1)	Adjust fp 
 16:    JMP  7,0(3)	Return 
* END FUNCTION inputb
* 
* ** ** ** ** ** ** ** ** ** ** ** **
* FUNCTION outputb
 17:     ST  3,-1(1)	Store return address 
 18:     LD  3,-2(1)	Load parameter 
 19:   OUTB  3,3,3	Output bool 
 20:     LD  3,-1(1)	Load return address 
 21:     LD  1,0(1)	Adjust fp 
 22:    JMP  7,0(3)	Return 
* END FUNCTION outputb
* 
* ** ** ** ** ** ** ** ** ** ** ** **
* FUNCTION inputc
 23:     ST  3,-1(1)	Store return address 
 24:    INC  2,2,2	Grab char input 
 25:     LD  3,-1(1)	Load return address 
 26:     LD  1,0(1)	Adjust fp 
 27:    JMP  7,0(3)	Return 
* END FUNCTION inputc
* 
* ** ** ** ** ** ** ** ** ** ** ** **
* FUNCTION outputc
 28:     ST  3,-1(1)	Store return address 
 29:     LD  3,-2(1)	Load parameter 
 30:   OUTC  3,3,3	Output char 
 31:     LD  3,-1(1)	Load return address 
 32:     LD  1,0(1)	Adjust fp 
 33:    JMP  7,0(3)	Return 
* END FUNCTION outputc
* 
* ** ** ** ** ** ** ** ** ** ** ** **
* FUNCTION outnl
 34:     ST  3,-1(1)	Store return address 
 35:  OUTNL  3,3,3	Output a newline 
 36:     LD  3,-1(1)	Load return address 
 37:     LD  1,0(1)	Adjust fp 
 38:    JMP  7,0(3)	Return 
* END FUNCTION outnl
* 
* ** ** ** ** ** ** ** ** ** ** ** **
* FUNCTION main
* TOFF set: -2
 39:     ST  3,-1(1)	Store return address 
* COMPOUND
* TOFF set: -3
* Compound Body
* EXPRESSION
 40:    LDC  3,10(6)	Load integer constant 
 41:     ST  3,-2(1)	Store variable i
* WHILE
 42:     LD  3,-2(1)	Load variable i
 43:     ST  3,-3(1)	Push left side 
* TOFF dec: -4
 44:    LDC  3,0(6)	Load integer constant 
* TOFF inc: -3
 45:     LD  4,-3(1)	Pop left into ac1 
 46:    TGT  3,4,3	Op > 
 47:    JNZ  3,1(7)	Jump to while part 
* DO
* COMPOUND
* TOFF set: -3
* Compound Body
* EXPRESSION
* CALL output
 49:     ST  1,-3(1)	Store fp in ghost frame for output
* TOFF dec: -4
* TOFF dec: -5
* Param 1
 50:     LD  3,-2(1)	Load variable i
 51:     ST  3,-5(1)	Push parameter 
* TOFF dec: -6
* Param end output
 52:    LDA  1,-3(1)	Ghost frame becomes new active frame 
 53:    LDA  3,1(7)	Return address in ac 
 54:    JMP  7,-49(7)	CALL output
 55:    LDA  3,0(2)	Save the result in ac 
* Call end output
* TOFF set: -3
* EXPRESSION
 56:     LD  3,-2(1)	load lhs variable i
 57:    LDA  3,-1(3)	decrement value of i
 58:     ST  3,-2(1)	Store variable i
* TOFF set: -3
* END COMPOUND
 59:    JMP  7,-18(7)	go to beginning of loop 
 48:    JMP  7,11(7)	Jump past loop [backpatch] 
* END WHILE
* EXPRESSION
* CALL outnl
 60:     ST  1,-3(1)	Store fp in ghost frame for outnl
* TOFF dec: -4
* TOFF dec: -5
* Param end outnl
 61:    LDA  1,-3(1)	Ghost frame becomes new active frame 
 62:    LDA  3,1(7)	Return address in ac 
 63:    JMP  7,-30(7)	CALL outnl
 64:    LDA  3,0(2)	Save the result in ac 
* Call end outnl
* TOFF set: -3
* WHILE
 65:     LD  3,-2(1)	Load variable i
 66:     ST  3,-3(1)	Push left side 
* TOFF dec: -4
 67:    LDC  3,10(6)	Load integer constant 
* TOFF inc: -3
 68:     LD  4,-3(1)	Pop left into ac1 
 69:    TLT  3,4,3	Op < 
 70:    JNZ  3,1(7)	Jump to while part 
* DO
* EXPRESSION
 72:     LD  3,-2(1)	Load variable i
 73:     ST  3,-3(1)	Push left side 
* TOFF dec: -4
 74:    LDC  3,1(6)	Load integer constant 
* TOFF inc: -3
 75:     LD  4,-3(1)	Pop left into ac1 
 76:    ADD  3,4,3	Op + 
 77:     ST  3,-2(1)	Store variable i
 78:    JMP  7,-14(7)	go to beginning of loop 
 71:    JMP  7,7(7)	Jump past loop [backpatch] 
* END WHILE
* EXPRESSION
* CALL output
 79:     ST  1,-3(1)	Store fp in ghost frame for output
* TOFF dec: -4
* TOFF dec: -5
* Param 1
 80:     LD  3,-2(1)	Load variable i
 81:     ST  3,-5(1)	Push parameter 
* TOFF dec: -6
* Param end output
 82:    LDA  1,-3(1)	Ghost frame becomes new active frame 
 83:    LDA  3,1(7)	Return address in ac 
 84:    JMP  7,-79(7)	CALL output
 85:    LDA  3,0(2)	Save the result in ac 
* Call end output
* TOFF set: -3
* TOFF set: -2
* END COMPOUND
* Add standard closing in case there is no return statement
 86:    LDC  2,0(6)	Set return value to 0 
 87:     LD  3,-1(1)	Load return address 
 88:     LD  1,0(1)	Adjust fp 
 89:    JMP  7,0(3)	Return 
* END FUNCTION main
  0:    JMP  7,89(7)	Jump to init [backpatch] 
* INIT
 90:    LDA  1,0(0)	set first frame at end of globals 
 91:     ST  1,0(1)	store old fp (point to self) 
* INIT GLOBALS AND STATICS
* END INIT GLOBALS AND STATICS
 92:    LDA  3,1(7)	Return address in ac 
 93:    JMP  7,-55(7)	Jump to main 
 94:   HALT  0,0,0	DONE! 
* END INIT
Number of warnings: 0
Number of errors: 0
