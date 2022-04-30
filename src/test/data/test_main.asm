.cpu _45gs02

.file [name="test.prg", segments="BasicUpstart,Code"]
.segmentdef BasicUpstart [start=$2001]
.segmentdef Code         [start=$2016]

.segment BasicUpstart "BasicUpstartMega65"
                .byte $09,$20 // End position
                .byte $72,$04 // Line number
                .byte $fe,$02,$30,$00 // BANK 0 command
                .byte <end, >end  // End of command marker (first byte after the 00 terminator)
                .byte $e2,$16 // Line number
                .byte $9e // SYS command
                .text toIntString(Entry)
                .byte $00
end:
	            	.byte $00,$00 //End of basic terminators


.segment Code "Main"
Entry: {
                // 40 MHz
                lda #65	
                sta $00			

                // Basepage = Zeropage
                lda #$00
                tab

                // Reset MAP Banking
                lda #$00
                tax
                tay
                taz
                map

                // All RAM + I/O
                lda #$35
                sta $01

                // Enable mega65 I/O personality (eg. for VIC IV registers)
                lda #$47
                sta $d02f
                lda #$53
                sta $d02f 

                // Disable CIA IRQs
                lda #$7f
                sta $dc0d
                sta $dd0d

                // Disable Raster IRQ
                lda #$70
                sta $d01a

                // End of MAP, enables IRQs again
                eom

                // Unbank C65 ROMs
                lda #%11111000
                trb $d030

                // Disable C65 ROM write protection via Hypervisor trap
                lda #$70
                sta $d640
                nop

                // Reset stack
                lda #$00
                tax
!:
                sta $0100,x
                inx
                beq !-
                ldx #$ff
                txs

!:
                lda #$12
                sta $02
                lda #$34
                sta $03

                jmp !-
}

                .fill 512, 0
