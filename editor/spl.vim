" Vim syntax file for simple programming language (spl)
" Language: Spl

if exists("b:current_syntax")
  finish
endif

syntax keyword Todos TODO NOTE FIXME

" Language keywords
syntax keyword Keywords include print const let fn memory if else while and or not eq neq lshift rshift sizeof store64 store32 store16 store8 load64 load32 load16 load8 enum alias struct cast static_assert

" Types
syntax keyword Types none any ptr cstr u64 i64 u32 i32 u16 i16 u8 i8

" Comments
syntax region CommentLine start="//" end="$"   contains=Todos

" Strings
syntax region String start=/\v"/ skip=/\v\\./ end=/\v"/
syntax region String start=/\v'/ skip=/\v\\./ end=/\v'/

" Values
syntax keyword Numbers NULL

" Numbers
" syntax match Numbers "\d\+\(u\=l\{0,2}\|ll\=u\)\>"
" syntax match Numbers "0x\x\+\(u\=l\{0,2}\|ll\=u\)\>"

" Set highlights
highlight default link Todos Todo
highlight default link Keywords Keyword
highlight default link CommentLine Comment
highlight default link String String
highlight default link Types Type
highlight default link Numbers Number

let b:current_syntax = "spl"
