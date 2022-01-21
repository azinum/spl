" Vim syntax file for simple programming language (spl)
" Language: Spl

if exists("b:current_syntax")
  finish
endif

syntax keyword Todos TODO NOTE FIXME

" Language keywords
syntax keyword Keywords print const let fn

" Comments
syntax region CommentLine start="//" end="$"   contains=Todos

" Strings
syntax region String start=/\v"/ skip=/\v\\./ end=/\v"/
syntax region String start=/\v'/ skip=/\v\\./ end=/\v'/

" Set highlights
highlight default link Todos Todo
highlight default link Keywords Keyword
highlight default link CommentLine Comment
highlight default link String String

let b:current_syntax = "spl"
