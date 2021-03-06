#ifndef YYERRCODE
#define YYERRCODE 256
#endif

#define BX_TOKEN_REG_AL 257
#define BX_TOKEN_REG_BL 258
#define BX_TOKEN_REG_CL 259
#define BX_TOKEN_REG_DL 260
#define BX_TOKEN_REG_AH 261
#define BX_TOKEN_REG_BH 262
#define BX_TOKEN_REG_CH 263
#define BX_TOKEN_REG_DH 264
#define BX_TOKEN_REG_AX 265
#define BX_TOKEN_REG_BX 266
#define BX_TOKEN_REG_CX 267
#define BX_TOKEN_REG_DX 268
#define BX_TOKEN_REG_EAX 269
#define BX_TOKEN_REG_EBX 270
#define BX_TOKEN_REG_ECX 271
#define BX_TOKEN_REG_EDX 272
#define BX_TOKEN_REG_SI 273
#define BX_TOKEN_REG_DI 274
#define BX_TOKEN_REG_BP 275
#define BX_TOKEN_REG_SP 276
#define BX_TOKEN_REG_IP 277
#define BX_TOKEN_REG_ESI 278
#define BX_TOKEN_REG_EDI 279
#define BX_TOKEN_REG_EBP 280
#define BX_TOKEN_REG_ESP 281
#define BX_TOKEN_REG_EIP 282
#define BX_TOKEN_CONTINUE 283
#define BX_TOKEN_STEPN 284
#define BX_TOKEN_STEP_OVER 285
#define BX_TOKEN_NEXT_STEP 286
#define BX_TOKEN_SET 287
#define BX_TOKEN_DEBUGGER 288
#define BX_TOKEN_LIST_BREAK 289
#define BX_TOKEN_VBREAKPOINT 290
#define BX_TOKEN_LBREAKPOINT 291
#define BX_TOKEN_PBREAKPOINT 292
#define BX_TOKEN_DEL_BREAKPOINT 293
#define BX_TOKEN_ENABLE_BREAKPOINT 294
#define BX_TOKEN_DISABLE_BREAKPOINT 295
#define BX_TOKEN_INFO 296
#define BX_TOKEN_QUIT 297
#define BX_TOKEN_PROGRAM 298
#define BX_TOKEN_REGISTERS 299
#define BX_TOKEN_CPU 300
#define BX_TOKEN_FPU 301
#define BX_TOKEN_ALL 302
#define BX_TOKEN_IDT 303
#define BX_TOKEN_GDT 304
#define BX_TOKEN_LDT 305
#define BX_TOKEN_TSS 306
#define BX_TOKEN_DIRTY 307
#define BX_TOKEN_LINUX 308
#define BX_TOKEN_CONTROL_REGS 309
#define BX_TOKEN_EXAMINE 310
#define BX_TOKEN_XFORMAT 311
#define BX_TOKEN_DISFORMAT 312
#define BX_TOKEN_SETPMEM 313
#define BX_TOKEN_SYMBOLNAME 314
#define BX_TOKEN_QUERY 315
#define BX_TOKEN_PENDING 316
#define BX_TOKEN_TAKE 317
#define BX_TOKEN_DMA 318
#define BX_TOKEN_IRQ 319
#define BX_TOKEN_DUMP_CPU 320
#define BX_TOKEN_SET_CPU 321
#define BX_TOKEN_DIS 322
#define BX_TOKEN_ON 323
#define BX_TOKEN_OFF 324
#define BX_TOKEN_DISASSEMBLE 325
#define BX_TOKEN_INSTRUMENT 326
#define BX_TOKEN_START 327
#define BX_TOKEN_STOP 328
#define BX_TOKEN_RESET 329
#define BX_TOKEN_PRINT 330
#define BX_TOKEN_LOADER 331
#define BX_TOKEN_STRING 332
#define BX_TOKEN_DOIT 333
#define BX_TOKEN_CRC 334
#define BX_TOKEN_TRACEON 335
#define BX_TOKEN_TRACEOFF 336
#define BX_TOKEN_PTIME 337
#define BX_TOKEN_TIMEBP_ABSOLUTE 338
#define BX_TOKEN_TIMEBP 339
#define BX_TOKEN_RECORD 340
#define BX_TOKEN_PLAYBACK 341
#define BX_TOKEN_MODEBP 342
#define BX_TOKEN_PRINT_STACK 343
#define BX_TOKEN_WATCH 344
#define BX_TOKEN_UNWATCH 345
#define BX_TOKEN_READ 346
#define BX_TOKEN_WRITE 347
#define BX_TOKEN_SHOW 348
#define BX_TOKEN_SYMBOL 349
#define BX_TOKEN_SYMBOLS 350
#define BX_TOKEN_LIST_SYMBOLS 351
#define BX_TOKEN_GLOBAL 352
#define BX_TOKEN_WHERE 353
#define BX_TOKEN_PRINT_STRING 354
#define BX_TOKEN_DIFF_MEMORY 355
#define BX_TOKEN_SYNC_MEMORY 356
#define BX_TOKEN_SYNC_CPU 357
#define BX_TOKEN_FAST_FORWARD 358
#define BX_TOKEN_PHY_2_LOG 359
#define BX_TOKEN_NUMERIC 360
#define BX_TOKEN_LONG_NUMERIC 361
#define BX_TOKEN_INFO_ADDRESS 362
#define BX_TOKEN_NE2000 363
#define BX_TOKEN_PIC 364
#define BX_TOKEN_PAGE 365
#define BX_TOKEN_CS 366
#define BX_TOKEN_ES 367
#define BX_TOKEN_SS 368
#define BX_TOKEN_DS 369
#define BX_TOKEN_FS 370
#define BX_TOKEN_GS 371
#define BX_TOKEN_FLAGS 372
#define BX_TOKEN_ALWAYS_CHECK 373
#define BX_TOKEN_TRACEREGON 374
#define BX_TOKEN_TRACEREGOFF 375
#define BX_TOKEN_HELP 376
#define BX_TOKEN_CALC 377
#define BX_TOKEN_RSHIFT 378
#define BX_TOKEN_LSHIFT 379
#define BX_TOKEN_IVT 380
#define NOT 381
#define NEG 382
typedef union {
  char    *sval;
  Bit32u   uval;
  Bit64u   ulval;
  bx_num_range   uval_range;
  Regs     reg;
  } YYSTYPE;
extern YYSTYPE bxlval;
