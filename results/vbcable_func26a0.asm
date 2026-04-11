FUNC_26A0 (4808 bytes)
1400026a0: mov      qword ptr [rsp + 0x10], rbx
1400026a5: push     rbp
1400026a6: push     rsi
1400026a7: push     rdi
1400026a8: push     r12
1400026aa: push     r13
1400026ac: push     r14
1400026ae: push     r15
1400026b0: lea      rbp, [rsp - 0x20]
1400026b5: sub      rsp, 0x120
1400026bc: mov      rax, qword ptr [rip + 0x1051d]
1400026c3: xor      rax, rsp
1400026c6: mov      qword ptr [rbp + 0x10], rax
1400026ca: mov      eax, dword ptr [rcx + 0x24]
1400026cd: mov      edi, r9d
1400026d0: movsxd   r12, dword ptr [rbp + 0x80]
1400026d7: mov      r10d, r8d
1400026da: mov      dword ptr [rsp + 0x48], r9d
1400026df: mov      r15, rcx
1400026e2: mov      dword ptr [rsp + 0x58], r8d
1400026e7: mov      qword ptr [rsp + 0x30], rdx
1400026ec: mov      qword ptr [rsp + 0x68], rcx
1400026f1: mov      dword ptr [rsp + 0x40], eax
1400026f5: test     eax, eax
1400026f7: jne      0x140002701
1400026f9: or       eax, 0xffffffff
1400026fc: jmp      0x140002a19
140002701: movsxd   rax, dword ptr [rcx + 8]
140002705: mov      r9d, dword ptr [rbp + 0x88]
14000270c: add      rax, r15
14000270f: mov      r8d, dword ptr [r15 + 0x20]
140002713: mov      qword ptr [rsp + 0x50], rax
140002718: mov      eax, dword ptr [rcx + 0x10]
14000271b: mov      ecx, r12d
14000271e: cmp      r12d, eax
140002721: mov      dword ptr [rsp + 0x20], eax
140002725: mov      dword ptr [rsp + 0x74], r8d
14000272a: cmovg    ecx, eax
14000272d: lea      rax, [r15 + 0x18]
140002731: mov      ebx, dword ptr [rax]
140002733: mov      qword ptr [rsp + 0x78], rax
140002738: mov      eax, dword ptr [r15 + 0x1c]
14000273c: sub      eax, ebx
14000273e: mov      dword ptr [rsp + 0x70], ecx
140002742: mov      ecx, dword ptr [r15 + 0x14]
140002746: test     eax, eax
140002748: mov      dword ptr [rsp + 0x24], ecx
14000274c: mov      dword ptr [rsp + 0x38], ebx
140002750: lea      r11d, [rax + rcx]
140002754: cmovg    r11d, eax
140002758: mov      eax, 8
14000275d: sub      r11d, 2
140002761: cmp      r9d, eax
140002764: je       0x14000278e
140002766: cmp      r9d, 0x10
14000276a: je       0x140002788
14000276c: cmp      r9d, 0x18
140002770: je       0x14000277c
140002772: mov      eax, 0xfffffffe
140002777: jmp      0x140002a19
14000277c: lea      ecx, [r12 + r12*2]
140002780: mov      eax, r10d
140002783: cdq      
140002784: idiv     ecx
140002786: jmp      0x140002795
140002788: lea      ecx, [r12 + r12]
14000278c: jmp      0x140002780
14000278e: mov      eax, r10d
140002791: cdq      
140002792: idiv     r12d
140002795: mov      r14d, eax
140002798: mov      esi, 0x12c
14000279d: mov      eax, 0x1b4e81b5
1400027a2: mov      dword ptr [rsp + 0x60], esi
1400027a6: imul     edi
1400027a8: mov      ecx, edi
1400027aa: mov      r13d, esi
1400027ad: sar      edx, 5
1400027b0: mov      eax, edx
1400027b2: shr      eax, 0x1f
1400027b5: add      edx, eax
1400027b7: imul     eax, edx, 0x12c
1400027bd: sub      ecx, eax
1400027bf: mov      dword ptr [rsp + 0x5c], ecx
1400027c3: jne      0x1400027e6
1400027c5: mov      eax, 0x1b4e81b5
1400027ca: imul     r8d
1400027cd: sar      edx, 5
1400027d0: mov      eax, edx
1400027d2: shr      eax, 0x1f
1400027d5: add      edx, eax
1400027d7: imul     ecx, edx, 0x12c
1400027dd: cmp      r8d, ecx
1400027e0: je       0x140002868
1400027e6: mov      eax, 0x51eb851f
1400027eb: mov      r13d, 0x64
1400027f1: imul     edi
1400027f3: mov      dword ptr [rsp + 0x60], r13d
1400027f8: sar      edx, 5
1400027fb: mov      eax, edx
1400027fd: shr      eax, 0x1f
140002800: add      edx, eax
140002802: imul     ecx, edx, 0x64
140002805: cmp      edi, ecx
140002807: jne      0x140002823
140002809: mov      eax, 0x51eb851f
14000280e: imul     r8d
140002811: sar      edx, 5
140002814: mov      eax, edx
140002816: shr      eax, 0x1f
140002819: add      edx, eax
14000281b: imul     ecx, edx, 0x64
14000281e: cmp      r8d, ecx
140002821: je       0x140002868
140002823: mov      eax, 0x1b4e81b5
140002828: mov      r13d, 0x4b
14000282e: imul     edi
140002830: mov      dword ptr [rsp + 0x60], r13d
140002835: sar      edx, 3
140002838: mov      eax, edx
14000283a: shr      eax, 0x1f
14000283d: add      edx, eax
14000283f: imul     ecx, edx, 0x4b
140002842: cmp      edi, ecx
140002844: jne      0x140002a14
14000284a: mov      eax, 0x1b4e81b5
14000284f: imul     r8d
140002852: sar      edx, 3
140002855: mov      eax, edx
140002857: shr      eax, 0x1f
14000285a: add      edx, eax
14000285c: imul     ecx, edx, 0x4b
14000285f: cmp      r8d, ecx
140002862: jne      0x140002a14
140002868: mov      eax, r8d
14000286b: cdq      
14000286c: idiv     r13d
14000286f: mov      r8d, eax
140002872: mov      dword ptr [rsp + 0x28], eax
140002876: mov      eax, edi
140002878: imul     r8d, r14d
14000287c: cdq      
14000287d: idiv     r13d
140002880: mov      ecx, eax
140002882: mov      eax, r8d
140002885: cdq      
140002886: idiv     ecx
140002888: inc      eax
14000288a: cmp      eax, r11d
14000288d: jle      0x1400028a0
14000288f: inc      dword ptr [r15 + 0x180]
140002896: mov      eax, 0xfffffffd
14000289b: jmp      0x140002a19
1400028a0: mov      r11d, dword ptr [rsp + 0x74]
1400028a5: xor      ecx, ecx
1400028a7: mov      r15d, ecx
1400028aa: mov      r13d, ecx
1400028ad: lea      edx, [rcx + 4]
1400028b0: cmp      edi, 0xac44  ; 44100Hz
1400028b6: jge      0x140002965
1400028bc: mov      eax, r11d
1400028bf: lea      r13d, [rcx + 2]
1400028c3: sar      eax, 1
1400028c5: cmp      edi, eax
1400028c7: mov      eax, r11d
1400028ca: cmovg    r13d, ecx
1400028ce: sar      eax, 2
1400028d1: cmp      edi, eax
1400028d3: mov      eax, r11d
1400028d6: cmovle   r13d, edx
1400028da: sar      eax, 3
1400028dd: cmp      edi, eax
1400028df: lea      eax, [rcx + 8]
1400028e2: cmovle   r13d, eax
1400028e6: mov      eax, r11d
1400028e9: sar      eax, 4
1400028ec: cmp      edi, eax
1400028ee: jg       0x140002965
1400028f0: movsxd   rdi, dword ptr [rsp + 0x70]
1400028f5: lea      r13d, [rcx + 0x10]
1400028f9: mov      qword ptr [rbp - 0x80], rdi
1400028fd: mov      esi, dword ptr [rsp + 0x48]
140002901: imul     esi, r13d
140002905: test     rdi, rdi
140002908: jle      0x140002932
14000290a: mov      rdx, qword ptr [rsp + 0x68]
14000290f: lea      rcx, [rbp - 0x70]
140002913: mov      r8, rdi
140002916: add      rdx, 0x13c
14000291d: shl      r8, 2
140002921: call     0x140007680
140002926: mov      r9d, dword ptr [rbp + 0x88]
14000292d: xor      ecx, ecx
14000292f: lea      edx, [rcx + 4]
140002932: movsxd   r8, dword ptr [rsp + 0x40]
140002937: mov      eax, ecx
140002939: mov      r11, qword ptr [rsp + 0x68]
14000293e: mov      r10d, 8
140002944: add      r8, r11
140002947: cmp      r13d, edx
14000294a: sete     al
14000294d: inc      eax
14000294f: cmp      r13d, r10d
140002952: jne      0x1400031e2
140002958: mov      dword ptr [rsp + 0x58], 3
140002960: jmp      0x1400031ed
140002965: movsxd   rdi, dword ptr [rsp + 0x70]
14000296a: mov      qword ptr [rbp - 0x80], rdi
14000296e: cmp      r13d, 1
140002972: jae      0x1400028fd
140002974: cmp      dword ptr [rsp + 0x5c], ecx
140002978: jne      0x14000299b
14000297a: mov      eax, 0x1b4e81b5
14000297f: imul     r11d
140002982: sar      edx, 5
140002985: mov      eax, edx
140002987: shr      eax, 0x1f
14000298a: add      edx, eax
14000298c: imul     ecx, edx, 0x12c
140002992: cmp      r11d, ecx
140002995: je       0x140002a40
14000299b: mov      r8d, dword ptr [rsp + 0x48]
1400029a0: mov      r14d, 0x51eb851f
1400029a6: mov      eax, r14d
1400029a9: mov      esi, 0x64
1400029ae: imul     r8d
1400029b1: sar      edx, 5
1400029b4: mov      eax, edx
1400029b6: shr      eax, 0x1f
1400029b9: add      edx, eax
1400029bb: imul     ecx, edx, 0x64
1400029be: cmp      r8d, ecx
1400029c1: jne      0x1400029db
1400029c3: mov      eax, r14d
1400029c6: imul     r11d
1400029c9: sar      edx, 5
1400029cc: mov      eax, edx
1400029ce: shr      eax, 0x1f
1400029d1: add      edx, eax
1400029d3: imul     ecx, edx, 0x64
1400029d6: cmp      r11d, ecx
1400029d9: je       0x140002a45
1400029db: mov      eax, 0x1b4e81b5
1400029e0: mov      esi, 0x4b
1400029e5: imul     r8d
1400029e8: sar      edx, 3
1400029eb: mov      eax, edx
1400029ed: shr      eax, 0x1f
1400029f0: add      edx, eax
1400029f2: imul     ecx, edx, 0x4b
1400029f5: cmp      r8d, ecx
1400029f8: jne      0x140002a14
1400029fa: mov      eax, 0x1b4e81b5
1400029ff: imul     r11d
140002a02: sar      edx, 3
140002a05: mov      eax, edx
140002a07: shr      eax, 0x1f
140002a0a: add      edx, eax
140002a0c: imul     ecx, edx, 0x4b
140002a0f: cmp      r11d, ecx
140002a12: je       0x140002a45
140002a14: mov      eax, 0xfffffe1a
140002a19: mov      rcx, qword ptr [rbp + 0x10]
140002a1d: xor      rcx, rsp
140002a20: call     0x140006f10
140002a25: mov      rbx, qword ptr [rsp + 0x168]
140002a2d: add      rsp, 0x120
140002a34: pop      r15
140002a36: pop      r14
140002a38: pop      r13
140002a3a: pop      r12
140002a3c: pop      rdi
140002a3d: pop      rsi
140002a3e: pop      rbp
140002a3f: ret      
140002a40: mov      r8d, dword ptr [rsp + 0x48]
140002a45: mov      eax, r8d
140002a48: xor      r8d, r8d
140002a4b: cdq      
140002a4c: idiv     esi
140002a4e: mov      r14d, eax
140002a51: mov      eax, r11d
140002a54: cdq      
140002a55: idiv     esi
140002a57: mov      dword ptr [rsp + 0x28], eax
140002a5b: mov      r13d, eax
140002a5e: mov      rax, qword ptr [rsp + 0x68]
140002a63: mov      esi, dword ptr [rax + 0x34]
140002a66: test     rdi, rdi
140002a69: jle      0x140002a8e
140002a6b: mov      r8, rdi
140002a6e: lea      rdx, [rax + 0x38]
140002a72: shl      r8, 2
140002a76: lea      rcx, [rbp - 0x70]
140002a7a: call     0x140007680
140002a7f: mov      r9d, dword ptr [rbp + 0x88]
140002a86: xor      r8d, r8d
140002a89: mov      r10d, dword ptr [rsp + 0x58]
140002a8e: mov      eax, 8
140002a93: cmp      r9d, eax
140002a96: je       0x140002f6b
140002a9c: cmp      r9d, 0x10
140002aa0: je       0x140002d2c
140002aa6: cmp      r9d, 0x18
140002aaa: je       0x140002ab6
140002aac: mov      eax, 0xfffffff7
140002ab1: jmp      0x140002a19
140002ab6: lea      ecx, [r12 + r12*2]
140002aba: mov      eax, r10d
140002abd: cdq      
140002abe: idiv     ecx
140002ac0: mov      r12d, eax
140002ac3: test     eax, eax
140002ac5: jle      0x1400031a5
140002acb: mov      rax, qword ptr [rsp + 0x30]
140002ad0: cmp      r14d, r13d
140002ad3: jle      0x140002bc5
140002ad9: movsxd   r11, ecx
140002adc: mov      ecx, r14d
140002adf: mov      r9d, r13d
140002ae2: sub      ecx, esi
140002ae4: cmp      ecx, r13d
140002ae7: cmovl    r9d, ecx
140002aeb: xor      edx, edx
140002aed: test     rdi, rdi
140002af0: jle      0x140002b38
140002af2: lea      r10, [rax + 1]
140002af6: movzx    eax, byte ptr [r10]
140002afa: movzx    ecx, byte ptr [r10 + 1]
140002aff: lea      r10, [r10 + 3]
140002b03: shl      ecx, 8
140002b06: or       ecx, eax
140002b08: movzx    eax, byte ptr [r10 - 4]
140002b0d: shl      ecx, 8
140002b10: or       ecx, eax
140002b12: mov      eax, r9d
140002b15: shl      ecx, 8
140002b18: sar      ecx, 0xd
140002b1b: imul     eax, ecx
140002b1e: cdq      
140002b1f: idiv     r13d
140002b22: add      dword ptr [rbp + r8*4 - 0x70], eax
140002b27: sub      ecx, eax
140002b29: mov      dword ptr [rbp + r8*4 - 0x30], ecx
140002b2e: inc      r8
140002b31: cmp      r8, rdi
140002b34: jl       0x140002af6
140002b36: xor      edx, edx
140002b38: add      esi, r9d
140002b3b: cmp      esi, r14d
140002b3e: jne      0x140002ba4
140002b40: mov      rcx, qword ptr [rsp + 0x50]
140002b45: mov      eax, ebx
140002b47: imul     eax, dword ptr [rsp + 0x20]
140002b4c: mov      r8, rdx
140002b4f: cdqe     
140002b51: lea      r10, [rcx + rax*4]
140002b55: test     rdi, rdi
140002b58: jle      0x140002b89
140002b5a: lea      rax, [rbp - 0x30]
140002b5e: sub      r10, rax
140002b61: mov      eax, r13d
140002b64: lea      rcx, [rbp - 0x30]
140002b68: imul     eax, dword ptr [rbp + r8*4 - 0x70]
140002b6e: lea      rcx, [rcx + r8*4]
140002b72: cdq      
140002b73: idiv     r14d
140002b76: mov      dword ptr [rcx + r10], eax
140002b7a: mov      eax, dword ptr [rcx]
140002b7c: mov      dword ptr [rbp + r8*4 - 0x70], eax
140002b81: inc      r8
140002b84: cmp      r8, rdi
140002b87: jl       0x140002b61
140002b89: inc      ebx
140002b8b: mov      esi, r13d
140002b8e: sub      esi, r9d
140002b91: inc      r15d
140002b94: cmp      ebx, dword ptr [rsp + 0x24]
140002b98: mov      r8d, 0
140002b9e: cmovge   ebx, r8d
140002ba2: jmp      0x140002ba7
140002ba4: xor      r8d, r8d
140002ba7: mov      rax, qword ptr [rsp + 0x30]
140002bac: dec      r12d
140002baf: add      rax, r11
140002bb2: mov      qword ptr [rsp + 0x30], rax
140002bb7: test     r12d, r12d
140002bba: jg       0x140002adc
140002bc0: jmp      0x1400031a1
140002bc5: movsxd   r10, ecx
140002bc8: mov      qword ptr [rsp + 0x40], r10
140002bcd: test     rdi, rdi
140002bd0: jle      0x140002c02
140002bd2: lea      rdx, [rax + 1]
140002bd6: movzx    eax, byte ptr [rdx]
140002bd9: movzx    ecx, byte ptr [rdx + 1]
140002bdd: lea      rdx, [rdx + 3]
140002be1: shl      ecx, 8
140002be4: or       ecx, eax
140002be6: movzx    eax, byte ptr [rdx - 4]
140002bea: shl      ecx, 8
140002bed: or       ecx, eax
140002bef: shl      ecx, 8
140002bf2: sar      ecx, 0xd
140002bf5: mov      dword ptr [rbp + r8*4 - 0x30], ecx
140002bfa: inc      r8
140002bfd: cmp      r8, rdi
140002c00: jl       0x140002bd6
140002c02: xor      r8d, r8d
140002c05: lea      r9d, [rsi + r13]
140002c09: test     esi, esi
140002c0b: jle      0x140002c86
140002c0d: mov      rcx, qword ptr [rsp + 0x50]
140002c12: mov      eax, r14d
140002c15: sub      eax, esi
140002c17: mov      r10d, r8d
140002c1a: mov      dword ptr [rsp + 0x60], eax
140002c1e: mov      eax, ebx
140002c20: imul     eax, dword ptr [rsp + 0x20]
140002c25: cdqe     
140002c27: lea      rsi, [rcx + rax*4]
140002c2b: test     rdi, rdi
140002c2e: jle      0x140002c6b
140002c30: mov      r13d, dword ptr [rsp + 0x60]
140002c35: lea      r11, [rbp - 0x30]
140002c39: sub      r11, rsi
140002c3c: lea      rcx, [rsi + r10*4]
140002c40: mov      r8d, dword ptr [rcx + r11]
140002c44: mov      eax, r8d
140002c47: imul     eax, r13d
140002c4b: cdq      
140002c4c: idiv     r14d
140002c4f: add      eax, dword ptr [rbp + r10*4 - 0x70]
140002c54: mov      dword ptr [rbp + r10*4 - 0x70], r8d
140002c59: inc      r10
140002c5c: mov      dword ptr [rcx], eax
140002c5e: cmp      r10, rdi
140002c61: jl       0x140002c3c
140002c63: mov      r13d, dword ptr [rsp + 0x28]
140002c68: xor      r8d, r8d
140002c6b: mov      r11d, dword ptr [rsp + 0x24]
140002c70: inc      ebx
140002c72: mov      r10, qword ptr [rsp + 0x40]
140002c77: sub      r9d, r14d
140002c7a: inc      r15d
140002c7d: cmp      ebx, r11d
140002c80: cmovge   ebx, r8d
140002c84: jmp      0x140002c8b
140002c86: mov      r11d, dword ptr [rsp + 0x24]
140002c8b: cmp      r9d, r14d
140002c8e: jl       0x140002ceb
140002c90: mov      r13d, dword ptr [rsp + 0x20]
140002c95: mov      r10, qword ptr [rsp + 0x50]
140002c9a: mov      eax, ebx
140002c9c: mov      rdx, r8
140002c9f: imul     eax, r13d
140002ca3: cdqe     
140002ca5: lea      rcx, [r10 + rax*4]
140002ca9: test     rdi, rdi
140002cac: jle      0x140002cca
140002cae: lea      r8, [rbp - 0x30]
140002cb2: sub      r8, rcx
140002cb5: mov      eax, dword ptr [r8 + rcx]
140002cb9: inc      rdx
140002cbc: mov      dword ptr [rcx], eax
140002cbe: lea      rcx, [rcx + 4]
140002cc2: cmp      rdx, rdi
140002cc5: jl       0x140002cb5
140002cc7: xor      r8d, r8d
140002cca: lea      eax, [rbx + 1]
140002ccd: sub      r9d, r14d
140002cd0: inc      r15d
140002cd3: mov      ebx, r8d
140002cd6: cmp      eax, r11d
140002cd9: cmovl    ebx, eax
140002cdc: cmp      r9d, r14d
140002cdf: jge      0x140002c9a
140002ce1: mov      r13d, dword ptr [rsp + 0x28]
140002ce6: mov      r10, qword ptr [rsp + 0x40]
140002ceb: mov      esi, r9d
140002cee: mov      rcx, r8
140002cf1: test     rdi, rdi
140002cf4: jle      0x140002d0e
140002cf6: mov      eax, r9d
140002cf9: imul     eax, dword ptr [rbp + rcx*4 - 0x30]
140002cfe: cdq      
140002cff: idiv     r14d
140002d02: mov      dword ptr [rbp + rcx*4 - 0x70], eax
140002d06: inc      rcx
140002d09: cmp      rcx, rdi
140002d0c: jl       0x140002cf6
140002d0e: mov      rax, qword ptr [rsp + 0x30]
140002d13: dec      r12d
140002d16: add      rax, r10
140002d19: mov      qword ptr [rsp + 0x30], rax
140002d1e: test     r12d, r12d
140002d21: jg       0x140002bcd
140002d27: jmp      0x1400031a1
140002d2c: mov      eax, r10d
140002d2f: lea      ecx, [r12 + r12]
140002d33: cdq      
140002d34: idiv     ecx
140002d36: mov      r11d, eax
140002d39: test     eax, eax
140002d3b: jle      0x1400031a5
140002d41: cmp      r14d, r13d
140002d44: jle      0x140002e1f
140002d4a: add      r12, r12
140002d4d: mov      qword ptr [rsp + 0x40], r12
140002d52: mov      ecx, r14d
140002d55: mov      r9d, r13d
140002d58: sub      ecx, esi
140002d5a: cmp      ecx, r13d
140002d5d: cmovl    r9d, ecx
140002d61: xor      edx, edx
140002d63: test     rdi, rdi
140002d66: jle      0x140002d9a
140002d68: mov      r12, qword ptr [rsp + 0x30]
140002d6d: movsx    ecx, word ptr [r12 + r8*2]
140002d72: mov      eax, r9d
140002d75: shl      ecx, 3
140002d78: imul     eax, ecx
140002d7b: cdq      
140002d7c: idiv     r13d
140002d7f: add      dword ptr [rbp + r8*4 - 0x70], eax
140002d84: sub      ecx, eax
140002d86: mov      dword ptr [rbp + r8*4 - 0x30], ecx
140002d8b: inc      r8
140002d8e: cmp      r8, rdi
140002d91: jl       0x140002d6d
140002d93: mov      r12, qword ptr [rsp + 0x40]
140002d98: xor      edx, edx
140002d9a: add      esi, r9d
140002d9d: cmp      esi, r14d
140002da0: jne      0x140002e06
140002da2: mov      rcx, qword ptr [rsp + 0x50]
140002da7: mov      eax, ebx
140002da9: imul     eax, dword ptr [rsp + 0x20]
140002dae: mov      r8, rdx
140002db1: cdqe     
140002db3: lea      r10, [rcx + rax*4]
140002db7: test     rdi, rdi
140002dba: jle      0x140002deb
140002dbc: lea      rax, [rbp - 0x30]
140002dc0: sub      r10, rax
140002dc3: mov      eax, r13d
140002dc6: lea      rcx, [rbp - 0x30]
140002dca: imul     eax, dword ptr [rbp + r8*4 - 0x70]
140002dd0: lea      rcx, [rcx + r8*4]
140002dd4: cdq      
140002dd5: idiv     r14d
140002dd8: mov      dword ptr [rcx + r10], eax
140002ddc: mov      eax, dword ptr [rcx]
140002dde: mov      dword ptr [rbp + r8*4 - 0x70], eax
140002de3: inc      r8
140002de6: cmp      r8, rdi
140002de9: jl       0x140002dc3
140002deb: inc      ebx
140002ded: mov      esi, r13d
140002df0: sub      esi, r9d
140002df3: inc      r15d
140002df6: cmp      ebx, dword ptr [rsp + 0x24]
140002dfa: mov      r8d, 0
140002e00: cmovge   ebx, r8d
140002e04: jmp      0x140002e09
140002e06: xor      r8d, r8d
140002e09: add      qword ptr [rsp + 0x30], r12
140002e0e: dec      r11d
140002e11: test     r11d, r11d
140002e14: jg       0x140002d52
140002e1a: jmp      0x1400031a1
140002e1f: mov      r10, r12
140002e22: add      r10, r10
140002e25: mov      qword ptr [rsp + 0x40], r10
140002e2a: mov      rcx, r8
140002e2d: test     rdi, rdi
140002e30: jle      0x140002e51
140002e32: mov      r13, qword ptr [rsp + 0x30]
140002e37: movsx    eax, word ptr [r13 + rcx*2]
140002e3d: shl      eax, 3
140002e40: mov      dword ptr [rbp + rcx*4 - 0x30], eax
140002e44: inc      rcx
140002e47: cmp      rcx, rdi
140002e4a: jl       0x140002e37
140002e4c: mov      r13d, dword ptr [rsp + 0x28]
140002e51: lea      r9d, [rsi + r13]
140002e55: test     esi, esi
140002e57: jle      0x140002ecf
140002e59: mov      rdx, qword ptr [rsp + 0x50]
140002e5e: mov      eax, ebx
140002e60: imul     eax, dword ptr [rsp + 0x20]
140002e65: mov      r12d, r14d
140002e68: sub      r12d, esi
140002e6b: mov      r10, r8
140002e6e: cdqe     
140002e70: test     rdi, rdi
140002e73: jle      0x140002eb6
140002e75: lea      rsi, [rbp - 0x30]
140002e79: lea      r13, [rdx + rax*4]
140002e7d: sub      rsi, r13
140002e80: lea      rcx, [r10*4]
140002e88: add      rcx, r13
140002e8b: mov      r8d, dword ptr [rcx + rsi]
140002e8f: mov      eax, r8d
140002e92: imul     eax, r12d
140002e96: cdq      
140002e97: idiv     r14d
140002e9a: add      eax, dword ptr [rbp + r10*4 - 0x70]
140002e9f: mov      dword ptr [rbp + r10*4 - 0x70], r8d
140002ea4: inc      r10
140002ea7: mov      dword ptr [rcx], eax
140002ea9: cmp      r10, rdi
140002eac: jl       0x140002e80
140002eae: mov      r13d, dword ptr [rsp + 0x28]
140002eb3: xor      r8d, r8d
140002eb6: mov      esi, dword ptr [rsp + 0x24]
140002eba: inc      ebx
140002ebc: mov      r10, qword ptr [rsp + 0x40]
140002ec1: sub      r9d, r14d
140002ec4: inc      r15d
140002ec7: cmp      ebx, esi
140002ec9: cmovge   ebx, r8d
140002ecd: jmp      0x140002ed3
140002ecf: mov      esi, dword ptr [rsp + 0x24]
140002ed3: cmp      r9d, r14d
140002ed6: jl       0x140002f32
140002ed8: mov      r13d, dword ptr [rsp + 0x20]
140002edd: mov      r10, qword ptr [rsp + 0x50]
140002ee2: mov      eax, ebx
140002ee4: mov      rdx, r8
140002ee7: imul     eax, r13d
140002eeb: cdqe     
140002eed: lea      rcx, [r10 + rax*4]
140002ef1: test     rdi, rdi
140002ef4: jle      0x140002f12
140002ef6: lea      r8, [rbp - 0x30]
140002efa: sub      r8, rcx
140002efd: mov      eax, dword ptr [rcx + r8]
140002f01: inc      rdx
140002f04: mov      dword ptr [rcx], eax
140002f06: lea      rcx, [rcx + 4]
140002f0a: cmp      rdx, rdi
140002f0d: jl       0x140002efd
140002f0f: xor      r8d, r8d
140002f12: lea      eax, [rbx + 1]
140002f15: sub      r9d, r14d
140002f18: inc      r15d
140002f1b: mov      ebx, r8d
140002f1e: cmp      eax, esi
140002f20: cmovl    ebx, eax
140002f23: cmp      r9d, r14d
140002f26: jge      0x140002ee2
140002f28: mov      r13d, dword ptr [rsp + 0x28]
140002f2d: mov      r10, qword ptr [rsp + 0x40]
140002f32: mov      esi, r9d
140002f35: mov      rcx, r8
140002f38: test     rdi, rdi
140002f3b: jle      0x140002f55
140002f3d: mov      eax, r9d
140002f40: imul     eax, dword ptr [rbp + rcx*4 - 0x30]
140002f45: cdq      
140002f46: idiv     r14d
140002f49: mov      dword ptr [rbp + rcx*4 - 0x70], eax
140002f4d: inc      rcx
140002f50: cmp      rcx, rdi
140002f53: jl       0x140002f3d
140002f55: add      qword ptr [rsp + 0x30], r10
140002f5a: dec      r11d
140002f5d: test     r11d, r11d
140002f60: jg       0x140002e2a
140002f66: jmp      0x1400031a1
140002f6b: mov      eax, r10d
140002f6e: cdq      
140002f6f: idiv     r12d
140002f72: mov      r11d, eax
140002f75: test     eax, eax
140002f77: jle      0x1400031a5
140002f7d: cmp      r14d, r13d
140002f80: jle      0x14000305b
140002f86: mov      qword ptr [rsp + 0x40], r12
140002f8b: mov      ecx, r14d
140002f8e: mov      r9d, r13d
140002f91: sub      ecx, esi
140002f93: cmp      ecx, r13d
140002f96: cmovl    r9d, ecx
140002f9a: xor      edx, edx
140002f9c: test     rdi, rdi
140002f9f: jle      0x140002fd6
140002fa1: mov      r12, qword ptr [rsp + 0x30]
140002fa6: movzx    ecx, byte ptr [r8 + r12]
140002fab: mov      eax, r9d
140002fae: add      ecx, -0x80
140002fb1: shl      ecx, 0xb
140002fb4: imul     eax, ecx
140002fb7: cdq      
140002fb8: idiv     r13d
140002fbb: add      dword ptr [rbp + r8*4 - 0x70], eax
140002fc0: sub      ecx, eax
140002fc2: mov      dword ptr [rbp + r8*4 - 0x30], ecx
140002fc7: inc      r8
140002fca: cmp      r8, rdi
140002fcd: jl       0x140002fa6
140002fcf: mov      r12, qword ptr [rsp + 0x40]
140002fd4: xor      edx, edx
140002fd6: add      esi, r9d
140002fd9: cmp      esi, r14d
140002fdc: jne      0x140003042
140002fde: mov      rcx, qword ptr [rsp + 0x50]
140002fe3: mov      eax, ebx
140002fe5: imul     eax, dword ptr [rsp + 0x20]
140002fea: mov      r8, rdx
140002fed: cdqe     
140002fef: lea      r10, [rcx + rax*4]
140002ff3: test     rdi, rdi
140002ff6: jle      0x140003027
140002ff8: lea      rax, [rbp - 0x30]
140002ffc: sub      r10, rax
140002fff: mov      eax, r13d
140003002: lea      rcx, [rbp - 0x30]
140003006: imul     eax, dword ptr [rbp + r8*4 - 0x70]
14000300c: lea      rcx, [rcx + r8*4]
140003010: cdq      
140003011: idiv     r14d
140003014: mov      dword ptr [rcx + r10], eax
140003018: mov      eax, dword ptr [rcx]
14000301a: mov      dword ptr [rbp + r8*4 - 0x70], eax
14000301f: inc      r8
140003022: cmp      r8, rdi
140003025: jl       0x140002fff
140003027: inc      ebx
140003029: mov      esi, r13d
14000302c: sub      esi, r9d
14000302f: inc      r15d
140003032: cmp      ebx, dword ptr [rsp + 0x24]
140003036: mov      r8d, 0
14000303c: cmovge   ebx, r8d
140003040: jmp      0x140003045
140003042: xor      r8d, r8d
140003045: add      qword ptr [rsp + 0x30], r12
14000304a: dec      r11d
14000304d: test     r11d, r11d
140003050: jg       0x140002f8b
140003056: jmp      0x1400031a1
14000305b: mov      r10, r12
14000305e: mov      qword ptr [rsp + 0x40], r12
140003063: mov      rcx, r8
140003066: test     rdi, rdi
140003069: jle      0x14000308c
14000306b: mov      r13, qword ptr [rsp + 0x30]
140003070: movzx    eax, byte ptr [rcx + r13]
140003075: add      eax, -0x80
140003078: shl      eax, 0xb
14000307b: mov      dword ptr [rbp + rcx*4 - 0x30], eax
14000307f: inc      rcx
140003082: cmp      rcx, rdi
140003085: jl       0x140003070
140003087: mov      r13d, dword ptr [rsp + 0x28]
14000308c: lea      r9d, [rsi + r13]
140003090: test     esi, esi
140003092: jle      0x14000310a
140003094: mov      rdx, qword ptr [rsp + 0x50]
140003099: mov      eax, ebx
14000309b: imul     eax, dword ptr [rsp + 0x20]
1400030a0: mov      r12d, r14d
1400030a3: sub      r12d, esi
1400030a6: mov      r10, r8
1400030a9: cdqe     
1400030ab: test     rdi, rdi
1400030ae: jle      0x1400030f1
1400030b0: lea      rsi, [rbp - 0x30]
1400030b4: lea      r13, [rdx + rax*4]
1400030b8: sub      rsi, r13
1400030bb: lea      rcx, [r10*4]
1400030c3: add      rcx, r13
1400030c6: mov      r8d, dword ptr [rcx + rsi]
1400030ca: mov      eax, r8d
1400030cd: imul     eax, r12d
1400030d1: cdq      
1400030d2: idiv     r14d
1400030d5: add      eax, dword ptr [rbp + r10*4 - 0x70]
1400030da: mov      dword ptr [rbp + r10*4 - 0x70], r8d
1400030df: inc      r10
1400030e2: mov      dword ptr [rcx], eax
1400030e4: cmp      r10, rdi
1400030e7: jl       0x1400030bb
1400030e9: mov      r13d, dword ptr [rsp + 0x28]
1400030ee: xor      r8d, r8d
1400030f1: mov      esi, dword ptr [rsp + 0x24]
1400030f5: inc      ebx
1400030f7: mov      r10, qword ptr [rsp + 0x40]
1400030fc: sub      r9d, r14d
1400030ff: inc      r15d
140003102: cmp      ebx, esi
140003104: cmovge   ebx, r8d
140003108: jmp      0x14000310e
14000310a: mov      esi, dword ptr [rsp + 0x24]
14000310e: cmp      r9d, r14d
140003111: jl       0x14000316d
140003113: mov      r13d, dword ptr [rsp + 0x20]
140003118: mov      r10, qword ptr [rsp + 0x50]
14000311d: mov      eax, ebx
14000311f: mov      rdx, r8
140003122: imul     eax, r13d
140003126: cdqe     
140003128: lea      rcx, [r10 + rax*4]
14000312c: test     rdi, rdi
14000312f: jle      0x14000314d
140003131: lea      r8, [rbp - 0x30]
140003135: sub      r8, rcx
140003138: mov      eax, dword ptr [rcx + r8]
14000313c: inc      rdx
14000313f: mov      dword ptr [rcx], eax
140003141: lea      rcx, [rcx + 4]
140003145: cmp      rdx, rdi
140003148: jl       0x140003138
14000314a: xor      r8d, r8d
14000314d: lea      eax, [rbx + 1]
140003150: sub      r9d, r14d
140003153: inc      r15d
140003156: mov      ebx, r8d
140003159: cmp      eax, esi
14000315b: cmovl    ebx, eax
14000315e: cmp      r9d, r14d
140003161: jge      0x14000311d
140003163: mov      r13d, dword ptr [rsp + 0x28]
140003168: mov      r10, qword ptr [rsp + 0x40]
14000316d: mov      esi, r9d
140003170: mov      rcx, r8
140003173: test     rdi, rdi
140003176: jle      0x140003190
140003178: mov      eax, r9d
14000317b: imul     eax, dword ptr [rbp + rcx*4 - 0x30]
140003180: cdq      
140003181: idiv     r14d
140003184: mov      dword ptr [rbp + rcx*4 - 0x70], eax
140003188: inc      rcx
14000318b: cmp      rcx, rdi
14000318e: jl       0x140003178
140003190: add      qword ptr [rsp + 0x30], r10
140003195: dec      r11d
140003198: test     r11d, r11d
14000319b: jg       0x140003063
1400031a1: mov      dword ptr [rsp + 0x38], ebx
1400031a5: mov      rdx, qword ptr [rsp + 0x68]
1400031aa: mov      dword ptr [rdx + 0x34], esi
1400031ad: test     rdi, rdi
1400031b0: jle      0x1400038e6
1400031b6: mov      rbx, qword ptr [rsp + 0x68]
1400031bb: lea      rax, [rbp - 0x70]
1400031bf: sub      rbx, rax
1400031c2: mov      rdx, r8
1400031c5: mov      eax, dword ptr [rbp + rdx*4 - 0x70]
1400031c9: lea      rcx, [rbx + rdx*4]
1400031cd: inc      rdx
1400031d0: mov      dword ptr [rbp + rcx - 0x38], eax
1400031d4: cmp      rdx, rdi
1400031d7: jl       0x1400031c5
1400031d9: mov      ebx, dword ptr [rsp + 0x38]
1400031dd: jmp      0x1400038e6
1400031e2: cmp      r13d, 0x10
1400031e6: cmove    eax, edx
1400031e9: mov      dword ptr [rsp + 0x58], eax
1400031ed: mov      edx, r14d
1400031f0: mov      dword ptr [rsp + 0x5c], edx
1400031f4: cmp      r9d, r10d
1400031f7: je       0x14000340f
1400031fd: cmp      r9d, 0x10
140003201: je       0x140003321
140003207: cmp      r9d, 0x18
14000320b: je       0x140003217
14000320d: mov      eax, 0xffffffce
140003212: jmp      0x140002a19
140003217: test     r14d, r14d
14000321a: jle      0x140003505
140003220: movsxd   r15, dword ptr [rsp + 0x20]
140003225: lea      eax, [r12 + r12*2]
140003229: movsxd   r12, eax
14000322c: lea      ebx, [r13 - 1]
140003230: mov      rax, qword ptr [rsp + 0x30]
140003235: shl      r15, 2
140003239: mov      r9, rcx
14000323c: test     rdi, rdi
14000323f: jle      0x140003273
140003241: lea      rdx, [rax + 1]
140003245: movzx    eax, byte ptr [rdx]
140003248: movzx    ecx, byte ptr [rdx + 1]
14000324c: lea      rdx, [rdx + 3]
140003250: shl      ecx, 8
140003253: or       ecx, eax
140003255: movzx    eax, byte ptr [rdx - 4]
140003259: shl      ecx, 8
14000325c: or       ecx, eax
14000325e: shl      ecx, 8
140003261: sar      ecx, 0xd
140003264: mov      dword ptr [rbp + r9*4 - 0x30], ecx
140003269: inc      r9
14000326c: cmp      r9, rdi
14000326f: jl       0x140003245
140003271: xor      ecx, ecx
140003273: mov      r9d, ecx
140003276: test     ebx, ebx
140003278: jle      0x1400032c7
14000327a: mov      r11d, ebx
14000327d: mov      r10, rcx
140003280: test     rdi, rdi
140003283: jle      0x1400032b9
140003285: mov      ebx, dword ptr [rsp + 0x58]
140003289: lea      edx, [r9 + 1]
14000328d: mov      eax, r11d
140003290: imul     edx, dword ptr [rbp + r10*4 - 0x30]
140003296: mov      ecx, ebx
140003298: imul     eax, dword ptr [rbp + r10*4 - 0x70]
14000329e: add      edx, eax
1400032a0: sar      edx, cl
1400032a2: mov      dword ptr [r8 + r10*4], edx
1400032a6: inc      r10
1400032a9: cmp      r10, rdi
1400032ac: jl       0x140003289
1400032ae: inc      r9d
1400032b1: lea      ebx, [r13 - 1]
1400032b5: xor      ecx, ecx
1400032b7: jmp      0x1400032bc
1400032b9: inc      r9d
1400032bc: add      r8, r15
1400032bf: dec      r11d
1400032c2: cmp      r9d, ebx
1400032c5: jl       0x14000327d
1400032c7: mov      rdx, rcx
1400032ca: test     rdi, rdi
1400032cd: jle      0x140003300
1400032cf: lea      r9, [rbp - 0x30]
1400032d3: mov      rcx, r8
1400032d6: sub      r9, r8
1400032d9: lea      r10, [rbp - 0x70]
1400032dd: sub      r10, r8
1400032e0: mov      ebx, 4
1400032e5: mov      eax, dword ptr [rcx + r9]
1400032e9: inc      rdx
1400032ec: mov      dword ptr [rcx], eax
1400032ee: mov      dword ptr [rcx + r10], eax
1400032f2: add      rcx, rbx
1400032f5: cmp      rdx, rdi
1400032f8: jl       0x1400032e5
1400032fa: lea      ebx, [r13 - 1]
1400032fe: xor      ecx, ecx
140003300: mov      rax, qword ptr [rsp + 0x30]
140003305: add      r8, r15
140003308: add      rax, r12
14000330b: dec      r14d
14000330e: mov      qword ptr [rsp + 0x30], rax
140003313: test     r14d, r14d
140003316: jg       0x140003239
14000331c: jmp      0x1400034f8
140003321: test     r14d, r14d
140003324: jle      0x140003505
14000332a: movsxd   r15, dword ptr [rsp + 0x20]
14000332f: lea      edx, [r13 - 1]
140003333: mov      rbx, qword ptr [rsp + 0x30]
140003338: shl      r15, 2
14000333c: add      r12, r12
14000333f: mov      qword ptr [rsp + 0x40], r12
140003344: xor      r9d, r9d
140003347: test     rdi, rdi
14000334a: jle      0x14000335f
14000334c: movsx    eax, word ptr [rbx + rcx*2]
140003350: shl      eax, 3
140003353: mov      dword ptr [rbp + rcx*4 - 0x30], eax
140003357: inc      rcx
14000335a: cmp      rcx, rdi
14000335d: jl       0x14000334c
14000335f: xor      ecx, ecx
140003361: test     edx, edx
140003363: jle      0x1400033b9
140003365: mov      r12d, dword ptr [rsp + 0x58]
14000336a: mov      r11d, edx
14000336d: mov      r10, rcx
140003370: test     rdi, rdi
140003373: jle      0x1400033a6
140003375: mov      edx, r11d
140003378: lea      eax, [r9 + 1]
14000337c: imul     edx, dword ptr [rbp + r10*4 - 0x70]
140003382: mov      ecx, r12d
140003385: imul     eax, dword ptr [rbp + r10*4 - 0x30]
14000338b: add      edx, eax
14000338d: sar      edx, cl
14000338f: mov      dword ptr [r8 + r10*4], edx
140003393: inc      r10
140003396: cmp      r10, rdi
140003399: jl       0x140003375
14000339b: inc      r9d
14000339e: lea      edx, [r13 - 1]
1400033a2: xor      ecx, ecx
1400033a4: jmp      0x1400033a9
1400033a6: inc      r9d
1400033a9: add      r8, r15
1400033ac: dec      r11d
1400033af: cmp      r9d, edx
1400033b2: jl       0x14000336d
1400033b4: mov      r12, qword ptr [rsp + 0x40]
1400033b9: mov      rdx, rcx
1400033bc: test     rdi, rdi
1400033bf: jle      0x1400033f4
1400033c1: lea      r9, [rbp - 0x30]
1400033c5: mov      rcx, r8
1400033c8: sub      r9, r8
1400033cb: lea      r10, [rbp - 0x70]
1400033cf: sub      r10, r8
1400033d2: mov      r12d, 4
1400033d8: mov      eax, dword ptr [rcx + r9]
1400033dc: inc      rdx
1400033df: mov      dword ptr [rcx], eax
1400033e1: mov      dword ptr [rcx + r10], eax
1400033e5: add      rcx, r12
1400033e8: cmp      rdx, rdi
1400033eb: jl       0x1400033d8
1400033ed: mov      r12, qword ptr [rsp + 0x40]
1400033f2: xor      ecx, ecx
1400033f4: add      r8, r15
1400033f7: lea      edx, [r13 - 1]
1400033fb: add      rbx, r12
1400033fe: dec      r14d
140003401: test     r14d, r14d
140003404: jg       0x140003344
14000340a: jmp      0x1400034f8
14000340f: test     r14d, r14d
140003412: jle      0x140003505
140003418: movsxd   r15, dword ptr [rsp + 0x20]
14000341d: lea      edx, [r13 - 1]
140003421: mov      rbx, qword ptr [rsp + 0x30]
140003426: shl      r15, 2
14000342a: mov      qword ptr [rsp + 0x40], r12
14000342f: xor      r9d, r9d
140003432: test     rdi, rdi
140003435: jle      0x14000344d
140003437: movzx    eax, byte ptr [rcx + rbx]
14000343b: add      eax, -0x80
14000343e: shl      eax, 0xb
140003441: mov      dword ptr [rbp + rcx*4 - 0x30], eax
140003445: inc      rcx
140003448: cmp      rcx, rdi
14000344b: jl       0x140003437
14000344d: xor      ecx, ecx
14000344f: test     edx, edx
140003451: jle      0x1400034a7
140003453: mov      r12d, dword ptr [rsp + 0x58]
140003458: mov      r11d, edx
14000345b: mov      r10, rcx
14000345e: test     rdi, rdi
140003461: jle      0x140003494
140003463: mov      edx, r11d
140003466: lea      eax, [r9 + 1]
14000346a: imul     edx, dword ptr [rbp + r10*4 - 0x70]
140003470: mov      ecx, r12d
140003473: imul     eax, dword ptr [rbp + r10*4 - 0x30]
140003479: add      edx, eax
14000347b: sar      edx, cl
14000347d: mov      dword ptr [r8 + r10*4], edx
140003481: inc      r10
140003484: cmp      r10, rdi
140003487: jl       0x140003463
140003489: inc      r9d
14000348c: lea      edx, [r13 - 1]
140003490: xor      ecx, ecx
140003492: jmp      0x140003497
140003494: inc      r9d
140003497: add      r8, r15
14000349a: dec      r11d
14000349d: cmp      r9d, edx
1400034a0: jl       0x14000345b
1400034a2: mov      r12, qword ptr [rsp + 0x40]
1400034a7: mov      rdx, rcx
1400034aa: test     rdi, rdi
1400034ad: jle      0x1400034e2
1400034af: lea      r9, [rbp - 0x30]
1400034b3: mov      rcx, r8
1400034b6: sub      r9, r8
1400034b9: lea      r10, [rbp - 0x70]
1400034bd: sub      r10, r8
1400034c0: mov      r12d, 4
1400034c6: mov      eax, dword ptr [rcx + r9]
1400034ca: inc      rdx
1400034cd: mov      dword ptr [rcx], eax
1400034cf: mov      dword ptr [rcx + r10], eax
1400034d3: add      rcx, r12
1400034d6: cmp      rdx, rdi
1400034d9: jl       0x1400034c6
1400034db: mov      r12, qword ptr [rsp + 0x40]
1400034e0: xor      ecx, ecx
1400034e2: add      r8, r15
1400034e5: lea      edx, [r13 - 1]
1400034e9: add      rbx, r12
1400034ec: dec      r14d
1400034ef: test     r14d, r14d
1400034f2: jg       0x14000342f
1400034f8: mov      ebx, dword ptr [rsp + 0x38]
1400034fc: mov      r11, qword ptr [rsp + 0x68]
140003501: mov      edx, dword ptr [rsp + 0x5c]
140003505: test     rdi, rdi
140003508: jle      0x14000351e
14000350a: mov      eax, dword ptr [rbp + rcx*4 - 0x70]
14000350e: mov      dword ptr [r11 + rcx*4 + 0x13c], eax
140003516: inc      rcx
140003519: cmp      rcx, rdi
14000351c: jl       0x14000350a
14000351e: movsxd   r14, dword ptr [r11 + 0x24]
140003522: xor      r8d, r8d
140003525: add      r14, r11
140003528: imul     r13d, edx
14000352c: cmp      esi, dword ptr [rsp + 0x74]
140003530: jne      0x1400035ed
140003536: mov      r15d, r13d
140003539: test     r13d, r13d
14000353c: jle      0x1400038e6
140003542: movsxd   rcx, dword ptr [rsp + 0x20]
140003547: mov      r12, rcx
14000354a: shl      r12, 2
14000354e: mov      eax, ebx
140003550: mov      rsi, r8
140003553: imul     eax, ecx
140003556: mov      r9d, 4
14000355c: mov      rcx, qword ptr [rsp + 0x50]
140003561: cdqe     
140003563: lea      rcx, [rcx + rax*4]
140003567: mov      eax, dword ptr [rsp + 0x70]
14000356b: test     eax, eax
14000356d: jle      0x1400035ab
14000356f: cmp      eax, r9d
140003572: jb       0x1400035ab
140003574: dec      eax
140003576: movsxd   rdx, eax
140003579: lea      rax, [r14 + rdx*4]
14000357d: lea      r8, [rcx + rdx*4]
140003581: cmp      rcx, rax
140003584: ja       0x14000358b
140003586: cmp      r8, r14
140003589: jae      0x1400035a8
14000358b: lea      r8, [rdi*4]
140003593: mov      rdx, r14
140003596: call     0x140007680
14000359b: inc      rsi
14000359e: cmp      rsi, rdi
1400035a1: jl       0x14000359b
1400035a3: xor      r8d, r8d
1400035a6: jmp      0x1400035c6
1400035a8: xor      r8d, r8d
1400035ab: cmp      rsi, rdi
1400035ae: jge      0x1400035c6
1400035b0: mov      rdx, r14
1400035b3: sub      rdx, rcx
1400035b6: mov      eax, dword ptr [rdx + rcx]
1400035b9: inc      rsi
1400035bc: mov      dword ptr [rcx], eax
1400035be: add      rcx, r9
1400035c1: cmp      rsi, rdi
1400035c4: jl       0x1400035b6
1400035c6: mov      esi, dword ptr [rsp + 0x24]
1400035ca: lea      eax, [rbx + 1]
1400035cd: mov      ecx, dword ptr [rsp + 0x20]
1400035d1: cmp      eax, esi
1400035d3: mov      ebx, r8d
1400035d6: cmovl    ebx, eax
1400035d9: add      r14, r12
1400035dc: dec      r13d
1400035df: test     r13d, r13d
1400035e2: jg       0x14000354e
1400035e8: jmp      0x1400038ea
1400035ed: mov      r9d, dword ptr [r11 + 0x34]
1400035f1: mov      eax, esi
1400035f3: cdq      
1400035f4: mov      r15d, r8d
1400035f7: idiv     dword ptr [rsp + 0x60]
1400035fb: mov      r10d, eax
1400035fe: test     rdi, rdi
140003601: jle      0x140003627
140003603: mov      rdx, r8
140003606: lea      rax, [rbp - 0x70]
14000360a: mov      r8, r11
14000360d: sub      r8, rax
140003610: lea      rax, [r8 + rdx*4]
140003614: mov      ecx, dword ptr [rbp + rax - 0x38]
140003618: mov      dword ptr [rbp + rdx*4 - 0x70], ecx
14000361c: inc      rdx
14000361f: cmp      rdx, rdi
140003622: jl       0x140003610
140003624: xor      r8d, r8d
140003627: test     r13d, r13d
14000362a: jle      0x1400038bf
140003630: mov      edx, dword ptr [rsp + 0x28]
140003634: cmp      r10d, edx
140003637: jle      0x140003739
14000363d: movsxd   rcx, dword ptr [rsp + 0x20]
140003642: mov      r12, rcx
140003645: shl      r12, 2
140003649: mov      qword ptr [rsp + 0x30], r12
14000364e: mov      eax, r10d
140003651: mov      r11d, edx
140003654: sub      eax, r9d
140003657: mov      rsi, r8
14000365a: cmp      eax, edx
14000365c: cmovl    r11d, eax
140003660: test     rdi, rdi
140003663: jle      0x1400036a9
140003665: lea      rax, [rbp - 0x70]
140003669: mov      r12, r14
14000366c: sub      r12, rax
14000366f: lea      rcx, [rbp - 0x70]
140003673: lea      rcx, [rcx + rsi*4]
140003677: mov      r8d, dword ptr [rcx + r12]
14000367b: mov      eax, r8d
14000367e: imul     eax, r11d
140003682: cdq      
140003683: idiv     dword ptr [rsp + 0x28]
140003687: add      dword ptr [rcx], eax
140003689: sub      r8d, eax
14000368c: mov      dword ptr [rbp + rsi*4 - 0x30], r8d
140003691: inc      rsi
140003694: cmp      rsi, rdi
140003697: jl       0x14000366f
140003699: mov      r12, qword ptr [rsp + 0x30]
14000369e: xor      r8d, r8d
1400036a1: mov      edx, dword ptr [rsp + 0x28]
1400036a5: mov      ecx, dword ptr [rsp + 0x20]
1400036a9: add      r9d, r11d
1400036ac: cmp      r9d, r10d
1400036af: jne      0x140003725
1400036b1: mov      r8, qword ptr [rsp + 0x50]
1400036b6: mov      eax, ebx
1400036b8: imul     eax, ecx
1400036bb: cdqe     
1400036bd: lea      r9, [r8 + rax*4]
1400036c1: xor      eax, eax
1400036c3: mov      r8d, eax
1400036c6: test     rdi, rdi
1400036c9: jle      0x14000370c
1400036cb: mov      r12d, dword ptr [rsp + 0x28]
1400036d0: lea      rax, [rbp - 0x30]
1400036d4: sub      r9, rax
1400036d7: mov      eax, r12d
1400036da: lea      rcx, [rbp - 0x30]
1400036de: imul     eax, dword ptr [rbp + r8*4 - 0x70]
1400036e4: lea      rcx, [rcx + r8*4]
1400036e8: cdq      
1400036e9: idiv     r10d
1400036ec: mov      dword ptr [rcx + r9], eax
1400036f0: mov      eax, dword ptr [rcx]
1400036f2: mov      dword ptr [rbp + r8*4 - 0x70], eax
1400036f7: inc      r8
1400036fa: cmp      r8, rdi
1400036fd: jl       0x1400036d7
1400036ff: mov      r12, qword ptr [rsp + 0x30]
140003704: mov      edx, dword ptr [rsp + 0x28]
140003708: mov      ecx, dword ptr [rsp + 0x20]
14000370c: inc      ebx
14000370e: mov      r9d, edx
140003711: sub      r9d, r11d
140003714: inc      r15d
140003717: cmp      ebx, dword ptr [rsp + 0x24]
14000371b: mov      r8d, 0
140003721: cmovge   ebx, r8d
140003725: add      r14, r12
140003728: dec      r13d
14000372b: test     r13d, r13d
14000372e: jg       0x14000364e
140003734: jmp      0x1400038ba
140003739: movsxd   rsi, dword ptr [rsp + 0x20]
14000373e: mov      r11d, dword ptr [rsp + 0x24]
140003743: mov      r12, rsi
140003746: shl      r12, 2
14000374a: mov      qword ptr [rsp + 0x30], r12
14000374f: cmp      r9d, r10d
140003752: jl       0x1400037b0
140003754: mov      r12, qword ptr [rsp + 0x50]
140003759: mov      eax, ebx
14000375b: mov      rdx, r8
14000375e: imul     eax, esi
140003761: cdqe     
140003763: lea      rcx, [r12 + rax*4]
140003767: test     rdi, rdi
14000376a: jle      0x140003790
14000376c: lea      r8, [rbp - 0x70]
140003770: mov      esi, 4
140003775: sub      r8, rcx
140003778: mov      eax, dword ptr [rcx + r8]
14000377c: inc      rdx
14000377f: mov      dword ptr [rcx], eax
140003781: add      rcx, rsi
140003784: cmp      rdx, rdi
140003787: jl       0x140003778
140003789: mov      esi, dword ptr [rsp + 0x20]
14000378d: xor      r8d, r8d
140003790: lea      eax, [rbx + 1]
140003793: sub      r9d, r10d
140003796: inc      r15d
140003799: mov      ebx, r8d
14000379c: cmp      eax, r11d
14000379f: cmovl    ebx, eax
1400037a2: cmp      r9d, r10d
1400037a5: jge      0x140003759
1400037a7: mov      r12, qword ptr [rsp + 0x30]
1400037ac: mov      edx, dword ptr [rsp + 0x28]
1400037b0: mov      rcx, r8
1400037b3: test     rdi, rdi
1400037b6: jle      0x1400037c8
1400037b8: mov      eax, dword ptr [r14 + rcx*4]
1400037bc: mov      dword ptr [rbp + rcx*4 - 0x30], eax
1400037c0: inc      rcx
1400037c3: cmp      rcx, rdi
1400037c6: jl       0x1400037b8
1400037c8: mov      ecx, edx
1400037ca: test     r9d, r9d
1400037cd: jle      0x140003832
1400037cf: mov      rcx, qword ptr [rsp + 0x50]
1400037d4: mov      eax, ebx
1400037d6: imul     eax, esi
1400037d9: mov      r11d, r10d
1400037dc: sub      r11d, r9d
1400037df: cdqe     
1400037e1: lea      rsi, [rcx + rax*4]
1400037e5: test     rdi, rdi
1400037e8: jle      0x140003812
1400037ea: mov      eax, r9d
1400037ed: mov      ecx, r11d
1400037f0: imul     eax, dword ptr [rbp + r8*4 - 0x70]
1400037f6: imul     ecx, dword ptr [rbp + r8*4 - 0x30]
1400037fc: add      eax, ecx
1400037fe: cdq      
1400037ff: idiv     r10d
140003802: mov      dword ptr [rsi + r8*4], eax
140003806: inc      r8
140003809: cmp      r8, rdi
14000380c: jl       0x1400037ea
14000380e: mov      edx, dword ptr [rsp + 0x28]
140003812: mov      esi, dword ptr [rsp + 0x20]
140003816: inc      ebx
140003818: mov      ecx, edx
14000381a: inc      r15d
14000381d: sub      ecx, r11d
140003820: mov      r8d, 0
140003826: mov      r11d, dword ptr [rsp + 0x24]
14000382b: cmp      ebx, r11d
14000382e: cmovge   ebx, r8d
140003832: cmp      ecx, r10d
140003835: jl       0x140003890
140003837: mov      r12, qword ptr [rsp + 0x50]
14000383c: mov      eax, ebx
14000383e: imul     eax, esi
140003841: cdqe     
140003843: lea      rdx, [r12 + rax*4]
140003847: test     rdi, rdi
14000384a: jle      0x14000386d
14000384c: lea      r9, [rbp - 0x30]
140003850: mov      esi, 4
140003855: sub      r9, rdx
140003858: mov      eax, dword ptr [rdx + r9]
14000385c: inc      r8
14000385f: mov      dword ptr [rdx], eax
140003861: add      rdx, rsi
140003864: cmp      r8, rdi
140003867: jl       0x140003858
140003869: mov      esi, dword ptr [rsp + 0x20]
14000386d: lea      eax, [rbx + 1]
140003870: xor      r8d, r8d
140003873: sub      ecx, r10d
140003876: inc      r15d
140003879: cmp      eax, r11d
14000387c: mov      ebx, r8d
14000387f: cmovl    ebx, eax
140003882: cmp      ecx, r10d
140003885: jge      0x14000383c
140003887: mov      r12, qword ptr [rsp + 0x30]
14000388c: mov      edx, dword ptr [rsp + 0x28]
140003890: mov      r9d, ecx
140003893: mov      rcx, r8
140003896: test     rdi, rdi
140003899: jle      0x1400038ab
14000389b: mov      eax, dword ptr [rbp + rcx*4 - 0x30]
14000389f: mov      dword ptr [rbp + rcx*4 - 0x70], eax
1400038a3: inc      rcx
1400038a6: cmp      rcx, rdi
1400038a9: jl       0x14000389b
1400038ab: add      r14, r12
1400038ae: dec      r13d
1400038b1: test     r13d, r13d
1400038b4: jg       0x14000374f
1400038ba: mov      r11, qword ptr [rsp + 0x68]
1400038bf: mov      dword ptr [r11 + 0x34], r9d
1400038c3: test     rdi, rdi
1400038c6: jle      0x1400038e6
1400038c8: lea      rax, [rbp - 0x70]
1400038cc: mov      rdx, r8
1400038cf: sub      r11, rax
1400038d2: mov      eax, dword ptr [rbp + rdx*4 - 0x70]
1400038d6: lea      rcx, [r11 + rdx*4]
1400038da: inc      rdx
1400038dd: mov      dword ptr [rbp + rcx - 0x38], eax
1400038e1: cmp      rdx, rdi
1400038e4: jl       0x1400038d2
1400038e6: mov      esi, dword ptr [rsp + 0x24]
1400038ea: mov      r12, qword ptr [rsp + 0x78]
1400038ef: cmp      dword ptr [rbp + 0x90], r8d
1400038f6: je       0x14000395d
1400038f8: mov      ecx, dword ptr [rsp + 0x20]
1400038fc: mov      edx, dword ptr [rsp + 0x70]
140003900: cmp      edx, ecx
140003902: jge      0x14000395d
140003904: mov      ebx, dword ptr [r12]
140003908: test     r15d, r15d
14000390b: jle      0x14000395d
14000390d: mov      r14, qword ptr [rbp - 0x80]
140003911: mov      eax, ecx
140003913: sub      eax, edx
140003915: movsxd   rdi, eax
140003918: shl      rdi, 2
14000391c: xor      r12d, r12d
14000391f: mov      eax, ebx
140003921: mov      r8, rdi
140003924: imul     eax, ecx
140003927: and      r8, 0xfffffffffffffffc
14000392b: xor      edx, edx
14000392d: movsxd   rcx, eax
140003930: mov      rax, qword ptr [rsp + 0x50]
140003935: add      rcx, r14
140003938: lea      rcx, [rax + rcx*4]
14000393c: call     0x140007940
140003941: mov      ecx, dword ptr [rsp + 0x20]
140003945: lea      eax, [rbx + 1]
140003948: cmp      eax, esi
14000394a: mov      ebx, r12d
14000394d: cmovl    ebx, eax
140003950: dec      r15d
140003953: test     r15d, r15d
140003956: jg       0x14000391f
140003958: mov      r12, qword ptr [rsp + 0x78]
14000395d: mov      dword ptr [r12], ebx
140003961: xor      eax, eax
140003963: jmp      0x140002a19