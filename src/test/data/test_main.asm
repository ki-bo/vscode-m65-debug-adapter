.cpu _45gs02

.file [name="test.prg", segments="BasicUpstart,Code"]
.segmentdef BasicUpstart [start=$2001]
.segmentdef Code         [start=$2016]

.segment BasicUpstart "BasicUpstartMega65"
                .const addr = *
                .byte $09,$20 // End position
                .byte $72,$04 // Line number
                .byte $fe,$02,$30,$00 // BANK 0 command
                .byte <end, >end  // End of command marker (first byte after the 00 terminator)
                .byte $e2,$16 // Line number
                .byte $9e // SYS command
                .var addressAsText = toIntString(addr)
                .text addressAsText
                .byte $00
end:
	            	.byte $00,$00 //End of basic terminators


.segment Code "Main"
Entry: {
                lda #$12
                sta $02
                lda #$34
                sta $03
!:
                jmp !-
}
