#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "y86asm.h"

line_t *y86bin_listhead = NULL;   /* the head of y86 binary code line list*/
line_t *y86bin_listtail = NULL;   /* the tail of y86 binary code line list*/
int y86asm_lineno = 0; /* the current line number of y86 assemble code */

#define err_print(_s, _a ...) do { \
  if (y86asm_lineno < 0) \
    fprintf(stderr, "[--]: "_s"\n", ## _a); \
  else \
    fprintf(stderr, "[L%d]: "_s"\n", y86asm_lineno, ## _a); \
} while (0);
// vitual memory
int vmaddr = 0;    /* vm addr */

/* register table */
reg_t reg_table[REG_CNT] = {
    {"%eax", REG_EAX},
    {"%ecx", REG_ECX},
    {"%edx", REG_EDX},
    {"%ebx", REG_EBX},
    {"%esp", REG_ESP},
    {"%ebp", REG_EBP},
    {"%esi", REG_ESI},
    {"%edi", REG_EDI},
};

regid_t find_register(char *name)
{  
    bool_t judge;
    int i;
    for(i=0;i<REG_CNT;i++){
      judge = !strncmp(name, reg_table[i].name, 4);
      if(judge)
	return reg_table[i].id;
    }
    return REG_ERR;
}

/* instruction set */
instr_t instr_set[] = {
    {"nop", 3,   HPACK(I_NOP, F_NONE), 1 },
    {"halt", 4,  HPACK(I_HALT, F_NONE), 1 },
    {"rrmovl", 6,HPACK(I_RRMOVL, F_NONE), 2 },
    {"cmovle", 6,HPACK(I_RRMOVL, C_LE), 2 },
    {"cmovl", 5, HPACK(I_RRMOVL, C_L), 2 },
    {"cmove", 5, HPACK(I_RRMOVL, C_E), 2 },
    {"cmovne", 6,HPACK(I_RRMOVL, C_NE), 2 },
    {"cmovge", 6,HPACK(I_RRMOVL, C_GE), 2 },
    {"cmovg", 5, HPACK(I_RRMOVL, C_G), 2 },
    {"irmovl", 6,HPACK(I_IRMOVL, F_NONE), 6 },
    {"rmmovl", 6,HPACK(I_RMMOVL, F_NONE), 6 },
    {"mrmovl", 6,HPACK(I_MRMOVL, F_NONE), 6 },
    {"addl", 4,  HPACK(I_ALU, A_ADD), 2 },
    {"subl", 4,  HPACK(I_ALU, A_SUB), 2 },
    {"andl", 4,  HPACK(I_ALU, A_AND), 2 },
    {"xorl", 4,  HPACK(I_ALU, A_XOR), 2 },
    {"jmp", 3,   HPACK(I_JMP, C_YES), 5 },
    {"jle", 3,   HPACK(I_JMP, C_LE), 5 },
    {"jl", 2,    HPACK(I_JMP, C_L), 5 },
    {"je", 2,    HPACK(I_JMP, C_E), 5 },
    {"jne", 3,   HPACK(I_JMP, C_NE), 5 },
    {"jge", 3,   HPACK(I_JMP, C_GE), 5 },
    {"jg", 2,    HPACK(I_JMP, C_G), 5 },
    {"call", 4,  HPACK(I_CALL, F_NONE), 5 },
    {"ret", 3,   HPACK(I_RET, F_NONE), 1 },
    {"pushl", 5, HPACK(I_PUSHL, F_NONE), 2 },
    {"popl", 4,  HPACK(I_POPL, F_NONE),  2 },
    {".byte", 5, HPACK(I_DIRECTIVE, D_DATA), 1 },
    {".word", 5, HPACK(I_DIRECTIVE, D_DATA), 2 },
    {".long", 5, HPACK(I_DIRECTIVE, D_DATA), 4 },
    {".pos", 4,  HPACK(I_DIRECTIVE, D_POS), 0 },
    {".align", 6,HPACK(I_DIRECTIVE, D_ALIGN), 0 },
    {NULL, 1,    0   , 0 } //end
};

instr_t *find_instr(char *name)
{
    int i=0;
    do{
      if(strncmp(name, instr_set[i].name, instr_set[i].len)==0)
         return &instr_set[i];
      i++;
    }while(instr_set[i].name != NULL);

    return NULL;
}

/* symbol table (don't forget to init and finit it) */
symbol_t *symtab = NULL;

/*
 * find_symbol: scan table to find the symbol
 * args
 *     name: the name of symbol
 *
 * return
 *     symbol_t: the 'name' symbol
 *     NULL: not exist
 */
symbol_t *find_symbol(char *name)
{
    //symbol_t *stemp = NULL;
    symbol_t *stemp = symtab->next;
    while(stemp != NULL){
      strcmp(stemp->name,name);
      if(!strcmp(stemp->name, name)){
	 return stemp;
      }
      stemp = stemp->next;  //next
    }
    return NULL;
}

/*
 * add_symbol: add a new symbol to the symbol table
 * args
 *     name: the name of symbol
 *
 * return
 *     0: success
 *     -1: error, the symbol has exist
 */
int add_symbol(char *name)  //remember to insert /0
{   
    /* check duplicate */ //check is the symbol has already existed 
    symbol_t *stemp = symtab->next;
    while(stemp != NULL){
      if(!strcmp(stemp->name, name)) return -1;  //the symbol has exist
      stemp = stemp->next;
    }
    
    /* create new symbol_t (don't forget to free it)*/
    symbol_t *n_symbol = (symbol_t *)malloc(sizeof(symbol_t));
    n_symbol->addr = vmaddr; 
    n_symbol->name = name;

    /* add the new symbol_t to symbol table */
    n_symbol->next = symtab->next;  //insert the new element just behind the list_head
    symtab->next = n_symbol;
    return 0;
}

/* relocation table (don't forget to init and finit it) */
reloc_t *reltab = NULL;

/*
 * add_reloc: add a new relocation to the relocation table
 * args
 *     name: the name of symbol
 *
 * return
 *     0: success
 *     -1: error, the symbol has exist
 */
void add_reloc(char *name, bin_t *bin) //void??
{
    /* create new reloc_t (don't forget to free it)*/
    reloc_t *n_reloc = (reloc_t *)malloc(sizeof(reloc_t));
    n_reloc->name = name;
    n_reloc->y86bin = bin;

    /* add the new reloc_t to relocation table */
    n_reloc->next = reltab->next;  //insert at the begining 
    reltab->next = n_reloc;
}


/* macro for parsing y86 assembly code */
#define IS_DIGIT(s) ((*(s)>='0' && *(s)<='9') || *(s)=='-' || *(s)=='+')
#define IS_LETTER(s) ((*(s)>='a' && *(s)<='z') || (*(s)>='A' && *(s)<='Z'))
#define IS_COMMENT(s) (*(s)=='#')
#define IS_REG(s) (*(s)=='%')
#define IS_IMM(s) (*(s)=='$')

#define IS_BLANK(s) (*(s)==' ' || *(s)=='\t')
#define IS_END(s) (*(s)=='\0')

#define SKIP_BLANK(s) do {  \
  while(!IS_END(s) && IS_BLANK(s))  \
    (s)++;    \
} while(0);

/* return value from different parse_xxx function */
typedef enum { PARSE_ERR=-1, PARSE_REG, PARSE_DIGIT, PARSE_SYMBOL, 
    PARSE_MEM, PARSE_DELIM, PARSE_INSTR, PARSE_LABEL} parse_t;

/*
 * parse_instr: parse an expected data token (e.g., 'rrmovl')
 * args
 *     ptr: point to the start of string
 *     inst: point to the instr_t within instr_set
 *
 * return
 *     PARSE_INSTR: success, move 'ptr' to the first char after token,
 *                            and store the pointer of the instruction to 'inst'
 *     PARSE_ERR: error, the value of 'ptr' and 'inst' are undefined
 */
parse_t parse_instr(char **ptr, instr_t **inst)
{
    /* skip the blank */
    char *str = *ptr;
    SKIP_BLANK(str);
    if(IS_END(str)) return PARSE_ERR;  //is this necessary??!!
    /* find_instr and check end */
    instr_t *i = find_instr(str);
    if(i == NULL ) return PARSE_ERR; // inst do not defined
    str += i->len;  //update str
    //check end
    if(!IS_END(str) && !IS_BLANK(str))
      return PARSE_ERR;
    /* set 'ptr' and 'inst' */
    *inst = i;
    *ptr = str;
    return PARSE_INSTR;
}

/*
 * parse_delim: parse an expected delimiter token (e.g., ',')
 * args
 *     ptr: point to the start of string
 *
 * return
 *     PARSE_DELIM: success, move 'ptr' to the first char after token
 *     PARSE_ERR: error, the value of 'ptr' and 'delim' are undefined
 */
parse_t parse_delim(char **ptr, char delim)
{
    /* skip the blank and check */
    char *str = *ptr;
    SKIP_BLANK(str);
    if(IS_END(str) || *str != delim) return PARSE_ERR;
    /* set 'ptr' */
    str++;
    *ptr = str;
    return PARSE_DELIM;
}

/*
 * parse_reg: parse an expected register token (e.g., '%eax')
 * args
 *     ptr: point to the start of string
 *     regid: point to the regid of register
 *
 * return
 *     PARSE_REG: success, move 'ptr' to the first char after token, 
 *                         and store the regid to 'regid'
 *     PARSE_ERR: error, the value of 'ptr' and 'regid' are undefined
 */
parse_t parse_reg(char **ptr, regid_t *regid)
{
    /* skip the blank and check */
    char *str = *ptr;
    SKIP_BLANK(str);
    if(!IS_REG(str)) return PARSE_ERR; //check   
    /* find register */
    regid_t r = find_register(str);
    if(r == REG_ERR) return PARSE_ERR;

    /* set 'ptr' and 'regid' */
    *regid = r;
    str += 4; //update str
    *ptr = str;
    return PARSE_REG;
}

/*
 * parse_symbol: parse an expected symbol token (e.g., 'Main')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *
 * return
 *     PARSE_SYMBOL: success, move 'ptr' to the first char after token,
 *                               and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr' and 'name' are undefined
 */
parse_t parse_symbol(char **ptr, char **name)
{
    /* skip the blank and check */
    char *str = *ptr;
    SKIP_BLANK(str);
    if(!IS_LETTER(str)) return PARSE_ERR; //check   
    /* allocate name and copy to it */
    int len = 0;
    while(IS_DIGIT(str + len) || IS_LETTER(str + len)){
      len++;
    }
    char *alc_name = (char *)malloc(len + 1);  //allocated name space ,the source of pointers(so many)
    alc_name[len] = '\0';  //terminationi mark
    
    int i;
    for(i=0;i<len;i++) alc_name[i] = str[i];
    str +=len;
    /* set 'ptr' and 'name' */
    *name = alc_name;
    *ptr = str;
    
    return PARSE_SYMBOL;
}

/*
 * parse_digit: parse an expected digit token (e.g., '0x100')
 * args
 *     ptr: point to the start of string
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, move 'ptr' to the first char after token
 *                            and store the value of digit to 'value'
 *     PARSE_ERR: error, the value of 'ptr' and 'value' are undefined
 */
parse_t parse_digit(char **ptr, long *value)
{
    /* skip the blank and check */
    char *str = *ptr;
    SKIP_BLANK(str);
    if(!IS_DIGIT(str)) return PARSE_ERR;
    /* calculate the digit, (NOTE: see strtoll()) */
    char *pEnd;  //write down the end position
    int base = 0;
    if(*(str+1) == 'x' && *str == '0')
      base = 16;
    else base = 10;
    long val = strtoll(str, &pEnd, base);
    str = pEnd;
    /* set 'ptr' and 'value' */
    *value = val;
    *ptr = str;
    return PARSE_DIGIT;
}

/*
 * parse_imm: parse an expected immediate token (e.g., '$0x100' or 'STACK')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, the immediate token is a digit,
 *                            move 'ptr' to the first char after token,
 *                            and store the value of digit to 'value'
 *     PARSE_SYMBOL: success, the immediate token is a symbol,
 *                            move 'ptr' to the first char after token,
 *                            and allocate and store name to 'name' 
 *     PARSE_ERR: error, the value of 'ptr', 'name' and 'value' are undefined
 */
parse_t parse_imm(char **ptr, char **name, long *value)
{
    /* skip the blank and check */
    char *str = *ptr;
    SKIP_BLANK(str);
    if(IS_END(str)) return PARSE_ERR;

    /* if IS_IMM, then parse the digit */
    if(IS_IMM(str)){
      str++;
      *ptr = str;
      return parse_digit(ptr, value);
    }

    /* if IS_LETTER, then parse the symbol */
    if(IS_LETTER(str)) return parse_symbol(ptr,name); 
    /* set 'ptr' and 'name' or 'value' */
    // the value of ptr and name is modified in other function thanks fof
    // the using of pointer to pointer    
}

/*
 * parse_mem: parse an expected memory token (e.g., '8(%ebp)')
 * args
 *     ptr: point to the start of string
 *     value: point to the value of digit
 *     regid: point to the regid of register
 *
 * return
 *     PARSE_MEM: success, move 'ptr' to the first char after token,
 *                          and store the value of digit to 'value',
 *                          and store the regid to 'regid'
 *     PARSE_ERR: error, the value of 'ptr', 'value' and 'regid' are undefined
 */
parse_t parse_mem(char **ptr, long *value, regid_t *regid)
{
    /* skip the blank and check */
    char *str = *ptr;
    SKIP_BLANK(str);
    if(IS_END(str)) return PARSE_ERR;
    *ptr = str;
    /* calculate the digit and register, (ex: (%ebp) or 8(%ebp)) */
    if(IS_DIGIT(str)){
      parse_digit(ptr, value);
    }
    else *value = 0; // do not have a digit in front of reg
    
    str = *ptr;  //update str pointer
    str++;
    *ptr = str;
    parse_reg(ptr, regid);
    
    str = *ptr;
    if(*str != ')') return PARSE_ERR;
    str++;
    *ptr = str;
    /* set 'ptr', 'value' and 'regid' */

    return PARSE_MEM;
}

/*
 * parse_data: parse an expected data token (e.g., '0x100' or 'array')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, data token is a digit,
 *                            and move 'ptr' to the first char after token,
 *                            and store the value of digit to 'value'
 *     PARSE_SYMBOL: success, data token is a symbol,
 *                            and move 'ptr' to the first char after token,
 *                            and allocate and store name to 'name' 
 *     PARSE_ERR: error, the value of 'ptr', 'name' and 'value' are undefined
 */
parse_t parse_data(char **ptr, char **name, long *value)
{
    /* skip the blank and check */
    char *str = ptr;
    SKIP_BLANK(str);
    if(IS_END(str)) return PARSE_ERR;
    *ptr = str;
    /* if IS_DIGIT, then parse the digit */
    if(IS_DIGIT(str)) return  parse_digit(ptr, value);
    /* if IS_LETTER, then parse the symbol */
    if(IS_LETTER(str)) return parse_symbol(ptr, name);
    /* set 'ptr', 'name' and 'value' */
    
}

/*
 * parse_label: parse an expected label token (e.g., 'Loop:')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *
 * return
 *     PARSE_LABEL: success, move 'ptr' to the first char after token
 *                            and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr' is undefined
 */
parse_t parse_label(char **ptr, char **name)
{
    /* skip the blank and check */
    char *str = *ptr;
    SKIP_BLANK(str);
    if(!IS_LETTER(str)) return PARSE_ERR;

    /* allocate name and copy to it */
    int len=0;
    while(IS_LETTER(str+len)||IS_DIGIT(str+len)){
       len++;
    }
    if(str[len] != ':') return PARSE_ERR;
    char *alc_name = (char *)malloc(len + 1);  //allocated name
    alc_name[len] = '\0';  //add termination mark
    int i;
    for(i=0;i<len;i++) alc_name[i] = str[i];
    str += (len+1);  //update str pointer

    /* set 'ptr' and 'name' */
    *name = alc_name;
    *ptr = str;
    return PARSE_LABEL;
}

/*
 * parse_line: parse a line of y86 code (e.g., 'Loop: mrmovl (%ecx), %esi')
 * (you could combine above parse_xxx functions to do it)
 * args
 *     line: point to a line_t data with a line of y86 assembly code
 *
 * return
 *     PARSE_XXX: success, fill line_t with assembled y86 code
 *     PARSE_ERR: error, try to print err information (e.g., instr type and line number)
 */
type_t parse_line(line_t *line)
{

/* when finish parse an instruction or lable, we still need to continue check 
* e.g., 
*  Loop: mrmovl (%ebp), %ecx
*           call SUM  #invoke SUM function */
    bin_t *y86bin = &line->y86bin;
    char *y86asm;
    y86asm = (char*)malloc((strlen(line->y86asm) + 1) * sizeof(char));
    strcpy(y86asm, line->y86asm);
    char *str = y86asm;
    char *label = NULL;
    instr_t *inst = NULL;
    
conti:
    /* skip blank and check IS_END */
    SKIP_BLANK(str);
    if(IS_END(str)) goto exit;
    
    /* is a comment ? */
    if(IS_COMMENT(str)) goto exit;
    /* is a label ? */
    if(parse_label(&str, &label) == PARSE_LABEL){
       if(add_symbol(label) == -1){
          line->type =TYPE_ERR;
	  err_print("Dup symbol:%s", label)  // the symbol has already exit
          goto exit;
       }
       line->y86bin.addr = vmaddr;
       line->type = TYPE_INS;
       goto conti;
    }
    
    
    /* is an instruction ? */
    if(parse_instr(&str, &inst) == PARSE_ERR){
       line->type = TYPE_ERR;
       err_print("Invalid instr");
       goto exit;
    }
    
    /* set type and y86bin */
    y86bin->addr = vmaddr;
    y86bin->bytes = inst->bytes; 
    y86bin->codes[0] = inst->code;
    line->type = TYPE_INS;
    
    /* update vmaddr */    
    int b_count = y86bin->bytes;

    vmaddr += b_count;
    /* parse the rest of instruction according to the itype */
    parse_t kind;
    long value;
    regid_t regA, regB;
    char *name;
    long *ptr; //used to modify code[i], write in valueC
    switch(HIGH(inst->code)){
       case I_RET: 
       case I_HALT: 
       case I_NOP:
          goto conti;
       
       case I_PUSHL: 
       case I_POPL:
	  // parse reg
	  if(parse_reg(&str, &regA) == PARSE_ERR){
	     line->type = TYPE_ERR;
	     err_print("Invalid REG");
	     goto exit;
	  }
          
	  // set reg code
          y86bin->codes[1] = HPACK(regA, 0xF);
	  goto conti;
       
       case I_JMP:
       case I_CALL:
	  if(parse_symbol(&str, &name) == PARSE_ERR){
	     line->type = TYPE_ERR;
	     err_print("Invalid DEST");
	     goto exit;
	  }

	  //add to reloc
	  add_reloc(name, y86bin);
          
	  goto conti;

       case I_IRMOVL:
          kind = parse_imm(&str, &name, &value);
	  if(kind == PARSE_ERR){
	     line->type = TYPE_ERR;
	     err_print("Invalid Immediate");
	     goto exit;
	  }
	  else if(kind == PARSE_SYMBOL){
	     value = 0;
	     add_reloc(name, y86bin);
	  }

	  //normal condition
	  ptr = (long *)&y86bin->codes[2];  //throw the spase into long
          *ptr = value; 

	  kind = parse_delim(&str, ',');
	  if(kind == PARSE_ERR){
	     line->type = TYPE_ERR;
	     err_print("Invalid ','");
	     goto exit;
	  }

	  kind = parse_reg(&str, &regB);
	  if(kind == PARSE_ERR){
	     line->type = TYPE_ERR;
	     err_print("Invalid REG");
	     goto exit;
	  }

	  y86bin->codes[1] = HPACK(0xF, regB);
	  goto conti;
       
       case I_RRMOVL:     //case that has rA & rB in order
       case I_ALU:    
	  kind = parse_reg(&str, &regA);
	  if(kind == PARSE_ERR){
	     line->type = TYPE_ERR;
	     err_print("Invalid REG");
	     goto exit;
	  }
          
          kind = parse_delim(&str, ',');
          
	  if(kind == PARSE_ERR){
	     line->type = TYPE_ERR;
	     err_print("Invalid ','");
	     goto exit;
	  }

	  kind = parse_reg(&str, &regB);
	  if(kind == PARSE_ERR){
	     line->type = TYPE_ERR;
	     err_print("Invalid REG");
	     goto exit;
	  }
    
          // write in code
	  y86bin->codes[1] = HPACK(regA,regB);
	  goto conti;
       
       case I_RMMOVL:
	  kind = parse_reg(&str, &regA);
          if(kind == PARSE_ERR){
	     line->type = TYPE_ERR;
	     err_print("Invalid REG");
	     goto exit;
	  }


          kind = parse_delim(&str, ',');
	  if(kind == PARSE_ERR){
	     line->type = TYPE_ERR;
	     err_print("Invalid ','");
	     goto exit;
	  }
          
	  kind = parse_mem(&str, &value, &regB);

	  if(kind == PARSE_ERR){
	     line->type = TYPE_ERR;
	     err_print("Invalid MEM");
	     goto exit;
	  }
          //write into code
	  y86bin->codes[1] = HPACK(regA, regB);
	  ptr =(long *)&y86bin->codes[2];
          *ptr = value;
	  goto conti;


       case I_MRMOVL:
	  kind = parse_mem(&str, &value, &regB);
	  if(kind == PARSE_ERR){
	     line->type = TYPE_ERR;
	     err_print("Invalid MEM");
	     goto exit;
	  }
          

          kind = parse_delim(&str, ',');
	  if(kind == PARSE_ERR){
	     line->type = TYPE_ERR;
	     err_print("Invalid ','");
	     goto exit;
	  }
    
    
	  kind = parse_reg(&str, &regA);
          if(kind == PARSE_ERR){
	     line->type = TYPE_ERR;
	     err_print("Invalid REG");
	     goto exit;
	  }

	  //write into code
	  ptr = (long *)&y86bin->codes[2];
	  *ptr = value;
	  y86bin->codes[1] = HPACK(regA,regB);

	  goto conti;
       
       case I_DIRECTIVE:
          switch (LOW(inst->code)){
	     case D_DATA:
		kind = parse_digit(&str, &value);
		if(kind == PARSE_ERR){
		   kind = parse_symbol(&str, &name);
		   if(kind == PARSE_ERR){
		      line->type = TYPE_ERR;
		      err_print("Invalid Data");
		      goto exit;
		   }

		   add_reloc(name, y86bin);
		}
                
		ptr =(long *)y86bin->codes;
		*ptr = value;
	        
		goto conti;

	     
	     case D_POS:
		kind = parse_digit(&str, &value);
		if(kind == PARSE_ERR){
		   line->type = TYPE_ERR;
		   err_print("Invalid Pos");
		   goto exit;
		}
                // modify position of binary code
		y86bin->addr =value;
		vmaddr = value;

		goto conti;
	     
	     case D_ALIGN:  //the action of align??!!
		kind = parse_digit(&str, &value);
		if(kind == PARSE_ERR){
		   line->type = TYPE_ERR;
		   err_print("Invalid align");
		   goto exit;
		}
                
		//modify position of binary code
		long remain = vmaddr % value;
		if(remain != 0) vmaddr += (value - remain);
		y86bin->addr = vmaddr;

		goto conti;
             //default is necessory??!1
	  }

	  //is default necessory??!!
    }

exit:
    free(y86asm);
    return line->type;
}

/*
 * assemble: assemble an y86 file (e.g., 'asum.ys')
 * args
 *     in: point to input file (an y86 assembly file)
 *
 * return
 *     0: success, assmble the y86 file to a list of line_t
 *     -1: error, try to print err information (e.g., instr type and line number)
 */
int assemble(FILE *in)
{
    static char asm_buf[MAX_INSLEN]; /* the current line of asm code */
    line_t *line;
    int slen;
    char *y86asm;

    /* read y86 code line-by-line, and parse them to generate raw y86 binary code list */
    while (fgets(asm_buf, MAX_INSLEN, in) != NULL) {
        slen  = strlen(asm_buf);
        if ((asm_buf[slen-1] == '\n') || (asm_buf[slen-1] == '\r')) { 
            asm_buf[--slen] = '\0'; /* replace terminator */
        } // !!important, a line will end with a \0

        /* store y86 assembly code */
        y86asm = (char *)malloc(sizeof(char) * (slen + 1)); // free in finit
        strcpy(y86asm, asm_buf);

        line = (line_t *)malloc(sizeof(line_t)); // free in finit
        memset(line, '\0', sizeof(line_t));

        /* set defualt */
        line->type = TYPE_COMM;
        line->y86asm = y86asm;
        line->next = NULL;

        /* add to y86 binary code list */
        y86bin_listtail->next = line;
        y86bin_listtail = line;
        y86asm_lineno ++;

        /* parse */
        if (parse_line(line) == TYPE_ERR)
            return -1;
    }

    /* skip line number information in err_print() */
    y86asm_lineno = -1;
    return 0;
}

/*
 * relocate: relocate the raw y86 binary code with symbol address
 *
 * return
 *     0: success
 *     -1: error, try to print err information (e.g., addr and symbol)
 */
int relocate(void)
{
    reloc_t *rtemp = NULL;
    
    rtemp = reltab->next;
    while (rtemp != NULL) {
        /* find symbol */
        symbol_t *symbol = find_symbol(rtemp->name);
        // this sentence is wrong; 
	if(symbol == NULL){  //can't find the name in symbol table
           err_print("Unknown symbol:'%s'", rtemp->name);
	   return -1;
	}
        /* relocate y86bin according itype */
        long *pointer;
	switch (HIGH(rtemp->y86bin->codes[0])){
	   case I_IRMOVL:
	      pointer = (long *)&rtemp->y86bin->codes[2];
	      break;  
	   case I_CALL:
	   case I_JMP:
	      pointer = (long *) &rtemp->y86bin->codes[1];
	      break;
	   default:
	      pointer = (long *) &rtemp->y86bin->codes[0];

	}
	*pointer = symbol->addr;
        
	/* next */
        rtemp = rtemp->next;
    }
    return 0;
}

/*
 * binfile: generate the y86 binary file
 * args
 *     out: point to output file (an y86 binary file)
 *
 * return
 *     0: success
 *     -1: error
 */
int binfile(FILE *out)
{
    /* prepare image with y86 binary code */
    line_t *line = y86bin_listhead;
    int pos =0;

    /* binary write y86 code to output file (NOTE: see fwrite()) */
    while(line){
       bool_t notEnd = 0;   //judge is this line the last binary code to write
       line_t *next_line = line;
       while(next_line){
          if(next_line->y86bin.bytes != 0){
	     notEnd = 1;
	     break;
	  }
          next_line = next_line->next;
       }
       
       if(pos < line->y86bin.addr && notEnd){  //fill the align part
	  int sub = line->y86bin.addr - pos;
	  int i;
	  for(i=0;i<sub;i++){
	   fprintf(out, "%c", 0); //fill in 0
	  }
	  pos = line->y86bin.addr; //update pos
       }
       fwrite(line->y86bin.codes, sizeof(byte_t), line->y86bin.bytes, out);
       
       pos += line->y86bin.bytes;
       line = line->next;
    }
    
    return 0;
}


/* whether print the readable output to screen or not ? */
bool_t screen = FALSE; 
// transform a digital number into hex form, len is the length of hex form
static void hexstuff(char *dest, int value, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        char c;
        int h = (value >> 4*i) & 0xF;
        c = h < 10 ? h + '0' : h - 10 + 'a';
        dest[len-i-1] = c;
    }
}

void print_line(line_t *line)
{
    char buf[26];

    /* line format: 0xHHH: cccccccccccc | <line> */
    if (line->type == TYPE_INS) {
        bin_t *y86bin = &line->y86bin;
        int i;
        
        strcpy(buf, "  0x000:              | ");
        
        hexstuff(buf+4, y86bin->addr, 3);
        if (y86bin->bytes > 0)
            for (i = 0; i < y86bin->bytes; i++)
                hexstuff(buf+9+2*i, y86bin->codes[i]&0xFF, 2);
    } else {
        strcpy(buf, "                      | ");
    }

    printf("%s%s\n", buf, line->y86asm);
}

/* 
 * print_screen: dump readable binary and assembly code to screen
 * (e.g., Figure 4.8 in ICS book)
 */
void print_screen(void)
{
    line_t *tmp = y86bin_listhead->next;
    
    /* line by line */
    while (tmp != NULL) {
        print_line(tmp);
        tmp = tmp->next;
    }
}

/* init and finit */
void init(void)
{
    reltab = (reloc_t *)malloc(sizeof(reloc_t)); // free in finit
    memset(reltab, 0, sizeof(reloc_t));

    symtab = (symbol_t *)malloc(sizeof(symbol_t)); // free in finit
    memset(symtab, 0, sizeof(symbol_t));

    y86bin_listhead = (line_t *)malloc(sizeof(line_t)); // free in finit
    memset(y86bin_listhead, 0, sizeof(line_t));
    y86bin_listtail = y86bin_listhead;
    y86asm_lineno = 0;
}

void finit(void)
{
    reloc_t *rtmp = NULL;
    do {
        rtmp = reltab->next;
        if (reltab->name) 
            free(reltab->name);
        free(reltab);
        reltab = rtmp;
    } while (reltab);
    
    symbol_t *stmp = NULL;
    do {
        stmp = symtab->next;
        if (symtab->name) 
            free(symtab->name);
        free(symtab);
        symtab = stmp;
    } while (symtab);

    line_t *ltmp = NULL;
    do {
        ltmp = y86bin_listhead->next;
        if (y86bin_listhead->y86asm) 
            free(y86bin_listhead->y86asm);
        free(y86bin_listhead);
        y86bin_listhead = ltmp;
    } while (y86bin_listhead);
}

static void usage(char *pname)
{
    printf("Usage: %s [-v] file.ys\n", pname);
    printf("   -v print the readable output to screen\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    int rootlen;
    char infname[512];
    char outfname[512];
    int nextarg = 1;
    FILE *in = NULL, *out = NULL;
    
    if (argc < 2)
        usage(argv[0]);
    
    if (argv[nextarg][0] == '-') {
        char flag = argv[nextarg][1];
        switch (flag) {
          case 'v':
            screen = TRUE;
            nextarg++;
            break;
          default:
            usage(argv[0]);
        }
    }

    /* parse input file name */
    rootlen = strlen(argv[nextarg])-3;
    /* only support the .ys file */
    if (strcmp(argv[nextarg]+rootlen, ".ys"))
        usage(argv[0]);
    
    if (rootlen > 500) {
        err_print("File name too long");
        exit(1);
    }


    /* init */
    init();

    
    /* assemble .ys file */
    strncpy(infname, argv[nextarg], rootlen);
    strcpy(infname+rootlen, ".ys");
    in = fopen(infname, "r");
    if (!in) {
        err_print("Can't open input file '%s'", infname);
        exit(1);
    }
    
    if (assemble(in) < 0) {
        err_print("Assemble y86 code error");
        fclose(in);
        exit(1);
    }
    fclose(in);


    /* relocate binary code */
    if (relocate() < 0) {
        err_print("Relocate binary code error");
        exit(1);
    }


    /* generate .bin file */
    strncpy(outfname, argv[nextarg], rootlen);
    strcpy(outfname+rootlen, ".bin");
    out = fopen(outfname, "wb");
    if (!out) {
        err_print("Can't open output file '%s'", outfname);
        exit(1);
    }

    if (binfile(out) < 0) {
        err_print("Generate binary file error");
        fclose(out);
        exit(1);
    }
    fclose(out);
    
    /* print to screen (.yo file) */
    if (screen)
       print_screen(); 

    /* finit */
    finit();
    return 0;
}


