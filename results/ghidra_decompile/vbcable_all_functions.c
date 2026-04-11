// === FUN_140001000 @ 140001000 (size=8) ===

void FUN_140001000(int param_1,int param_2,undefined4 param_3)

{
  FUN_140001008(param_1,param_2,param_3,(longlong *)0x0);
  return;
}



// === FUN_140001008 @ 140001008 (size=180) ===

longlong * FUN_140001008(int param_1,int param_2,undefined4 param_3,longlong *param_4)

{
  int iVar1;
  
  iVar1 = param_1 * param_2;
  if (param_4 == (longlong *)0x0) {
    param_4 = FUN_140003a04(0x40,(undefined1 *)(longlong)(iVar1 * 0xc + 400));
    *(undefined4 *)(param_4 + 6) = 1;
  }
  else {
    *(undefined4 *)(param_4 + 6) = 0;
  }
  *(undefined4 *)((longlong)param_4 + 0x24) = 0;
  *(int *)((longlong)param_4 + 0xc) = param_1;
  *(int *)(param_4 + 2) = param_2;
  *(undefined4 *)(param_4 + 4) = param_3;
  *(int *)(param_4 + 5) = iVar1 * 4;
  *(int *)((longlong)param_4 + 0x2c) = iVar1 * 8;
  *(undefined4 *)(param_4 + 1) = 400;
  if (0 < iVar1 * 8) {
    *(int *)((longlong)param_4 + 0x24) = iVar1 * 4 + 400;
  }
  *(undefined4 *)((longlong)param_4 + 4) = param_3;
  *(int *)param_4 = param_1;
  *(int *)((longlong)param_4 + 0x14) = param_1;
  FUN_1400039ac((longlong)param_4);
  return param_4;
}



// === FUN_1400010bc @ 1400010bc (size=45) ===

undefined8 FUN_1400010bc(longlong param_1)

{
  undefined8 uVar1;
  
  if (param_1 == 0) {
    uVar1 = 0xffffffff;
  }
  else if (*(int *)(param_1 + 0x30) == 0) {
    uVar1 = 1;
  }
  else {
    ExFreePoolWithTag(param_1,0x4456534d);
    uVar1 = 0;
  }
  return uVar1;
}



// === FUN_1400010ec @ 1400010ec (size=64) ===

undefined8 FUN_1400010ec(longlong param_1)

{
  int iVar1;
  int iVar2;
  int iVar3;
  ulonglong uVar4;
  
  if (param_1 == 0) {
    return 0xffffffff;
  }
  iVar2 = *(int *)(param_1 + 0x1c);
  iVar1 = *(int *)(param_1 + 0x18) - iVar2;
  iVar3 = iVar1 + *(int *)(param_1 + 0x14);
  if (-1 < iVar1) {
    iVar3 = iVar1;
  }
  if (0 < (int)(iVar3 - 1U)) {
    uVar4 = (ulonglong)(iVar3 - 1U);
    do {
      iVar3 = iVar2 + 1;
      iVar2 = 0;
      if (iVar3 < *(int *)(param_1 + 0x14)) {
        iVar2 = iVar3;
      }
      uVar4 = uVar4 - 1;
    } while (uVar4 != 0);
    *(int *)(param_1 + 0x1c) = iVar2;
  }
  return 0;
}



// === FUN_14000112c @ 14000112c (size=22) ===

int FUN_14000112c(longlong param_1)

{
  int iVar1;
  
  if (*(int *)(param_1 + 8) == 0) {
    return -1;
  }
  iVar1 = *(int *)(param_1 + 0x18) - *(int *)(param_1 + 0x1c);
  if (iVar1 < 0) {
    iVar1 = iVar1 + *(int *)(param_1 + 0x14);
  }
  return iVar1;
}



// === FUN_140001144 @ 140001144 (size=58) ===

ulonglong FUN_140001144(longlong param_1,int param_2)

{
  ulonglong uVar1;
  uint uVar2;
  
  if (*(int *)(param_1 + 8) == 0) {
    return 0xffffffff;
  }
  uVar2 = *(int *)(param_1 + 0x18) - *(int *)(param_1 + 0x1c);
  if ((int)uVar2 < 0) {
    uVar2 = uVar2 + *(int *)(param_1 + 0x14);
  }
  uVar1 = (ulonglong)uVar2;
  if (0 < *(int *)(param_1 + 0x20)) {
    uVar1 = ((longlong)(int)uVar2 * (longlong)param_2) / (longlong)*(int *)(param_1 + 0x20);
  }
  return uVar1 & 0xffffffff;
}



// === FUN_140001180 @ 140001180 (size=81) ===

void FUN_140001180(int *param_1,int param_2)

{
  int iVar1;
  int iVar2;
  
  iVar2 = param_1[5];
  iVar1 = *param_1;
  if (iVar2 != iVar1) {
    if (iVar2 < iVar1) {
      iVar2 = param_1[3];
      if (iVar1 <= param_1[3]) {
        iVar2 = iVar1;
      }
    }
    else {
      iVar2 = iVar1;
      if (iVar1 < 0x30) {
        iVar2 = 0x30;
      }
    }
    param_1[5] = iVar2;
  }
  if (param_1[8] != param_1[1]) {
    param_1[8] = param_1[1];
  }
  if (param_2 == 0) {
    if (iVar2 <= param_1[7]) {
      param_1[7] = 0;
    }
  }
  else if (iVar2 <= param_1[6]) {
    param_1[6] = 0;
    return;
  }
  return;
}



// === FUN_1400011d4 @ 1400011d4 (size=1493) ===

ulonglong FUN_1400011d4(int *param_1,longlong *param_2,int param_3,int param_4,uint param_5,
                       int param_6,uint param_7)

{
  uint uVar1;
  int iVar2;
  uint uVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  int iVar7;
  int iVar8;
  byte bVar9;
  uint uVar10;
  ulonglong uVar11;
  undefined1 *puVar12;
  char *pcVar13;
  int iVar14;
  ulonglong uVar15;
  int *piVar16;
  longlong lVar17;
  uint uVar18;
  longlong lVar19;
  ulonglong uVar20;
  undefined4 *puVar21;
  undefined2 *puVar22;
  ulonglong uVar23;
  uint uVar24;
  ulonglong uVar25;
  
  if (param_1[2] == 0) {
    return 0xffffffff;
  }
  if (param_3 == 0) {
    return 0xfffffffe;
  }
  uVar15 = (ulonglong)(int)param_5;
  if ((int)param_5 < 1) {
    return 0xfffffffd;
  }
  if (param_4 < 8000) {
    return 0xfffffffc;
  }
  if (200000 < param_4) {
    return 0xfffffffb;
  }
  FUN_140001180(param_1,0);
  if ((0 < param_1[8]) && (param_1[8] != param_4)) {
    uVar15 = FUN_1400017ac((longlong)param_1,param_2,param_3,param_4,param_5,param_6,param_7);
    return uVar15;
  }
  uVar1 = param_1[4];
  iVar5 = param_1[7];
  iVar2 = param_1[5];
  uVar3 = param_5;
  if ((int)uVar1 < (int)param_5) {
    uVar3 = uVar1;
  }
  iVar4 = param_1[6] - iVar5;
  iVar14 = iVar4 + iVar2;
  if (-1 < iVar4) {
    iVar14 = iVar4;
  }
  iVar4 = param_1[2];
  uVar18 = 0;
  if ((param_7 & 0x100000) == 0) {
    uVar18 = param_7;
  }
  if (param_1[0x62] == 0) {
LAB_1400013e4:
    uVar11 = 0;
    lVar17 = (longlong)(int)uVar3;
    iVar8 = iVar5;
    if (param_6 == 8) {
      iVar6 = (int)((longlong)param_3 / (longlong)(int)param_5);
      if (iVar6 <= iVar14) {
        uVar20 = uVar11;
        if (0 < (int)uVar3) {
          do {
            pcVar13 = (char *)((longlong)(int)uVar11 + (longlong)param_2);
            iVar8 = iVar5;
            if (0 < iVar6) {
              uVar23 = (longlong)param_3 / (longlong)(int)param_5 & 0xffffffff;
              do {
                *pcVar13 = (char)(*(int *)((longlong)param_1 +
                                          ((longlong)(int)(iVar8 * uVar1) + uVar20) * 4 +
                                          (longlong)iVar4) >> 0xb) + -0x80;
                iVar14 = iVar8 + 1;
                iVar8 = 0;
                if (iVar14 < iVar2) {
                  iVar8 = iVar14;
                }
                pcVar13 = pcVar13 + (int)uVar15;
                uVar23 = uVar23 - 1;
              } while (uVar23 != 0);
              uVar15 = (ulonglong)param_5;
            }
            uVar11 = (ulonglong)((int)uVar11 + 1);
            uVar20 = uVar20 + 1;
          } while ((longlong)uVar20 < lVar17);
        }
        goto LAB_14000178b;
      }
      param_1[0x61] = param_1[0x61] + 1;
      if ((uVar18 != 0) && (0 < (int)(iVar6 * param_5))) {
        FUN_140007940(param_2,0x80,(undefined1 *)(longlong)(int)(iVar6 * param_5));
      }
      uVar15 = 0xfffffffb;
    }
    else if (param_6 == 0x10) {
      uVar20 = (longlong)param_3 / (longlong)(int)(param_5 * 2);
      iVar6 = (int)uVar20;
      if (iVar6 <= iVar14) {
        uVar23 = uVar11;
        if (0 < (int)uVar3) {
          do {
            puVar22 = (undefined2 *)((longlong)param_2 + (longlong)(int)uVar23 * 2);
            uVar25 = uVar20 & 0xffffffff;
            iVar8 = iVar5;
            iVar14 = iVar5;
            if (0 < iVar6) {
              do {
                *puVar22 = (short)(*(int *)((longlong)param_1 +
                                           ((longlong)(int)(iVar14 * uVar1) + uVar11) * 4 +
                                           (longlong)iVar4) >> 3);
                iVar8 = 0;
                if (iVar14 + 1 < iVar2) {
                  iVar8 = iVar14 + 1;
                }
                puVar22 = puVar22 + uVar15;
                uVar25 = uVar25 - 1;
                iVar14 = iVar8;
              } while (uVar25 != 0);
            }
            uVar11 = uVar11 + 1;
            uVar23 = (ulonglong)((int)uVar23 + 1);
          } while ((longlong)uVar11 < lVar17);
        }
        goto LAB_14000178b;
      }
      param_1[0x61] = param_1[0x61] + 1;
      if ((uVar18 != 0) && (iVar5 = iVar6 * param_5 * 2, 0 < iVar5)) {
        FUN_140007940(param_2,0,(undefined1 *)(longlong)iVar5);
      }
      uVar15 = 0xfffffffa;
    }
    else {
      if (param_6 == 0x18) {
        uVar15 = (longlong)param_3 / (longlong)(int)(param_5 * 3);
        iVar6 = (int)uVar15;
        if (iVar6 <= iVar14) {
          if (0 < lVar17) {
            lVar19 = 0;
            iVar14 = 0;
            do {
              puVar12 = (undefined1 *)((longlong)iVar14 + (longlong)param_2);
              iVar8 = iVar5;
              if (0 < iVar6) {
                uVar11 = uVar15 & 0xffffffff;
                do {
                  iVar7 = *(int *)((longlong)param_1 +
                                  ((int)(iVar8 * uVar1) + lVar19) * 4 + (longlong)iVar4) << 5;
                  *puVar12 = (char)iVar7;
                  puVar12[1] = (char)((uint)iVar7 >> 8);
                  puVar12[2] = (char)((uint)iVar7 >> 0x10);
                  iVar7 = iVar8 + 1;
                  iVar8 = 0;
                  if (iVar7 < iVar2) {
                    iVar8 = iVar7;
                  }
                  puVar12 = puVar12 + (int)(param_5 * 3);
                  uVar11 = uVar11 - 1;
                } while (uVar11 != 0);
              }
              iVar14 = iVar14 + 3;
              lVar19 = lVar19 + 1;
            } while (lVar19 < lVar17);
          }
LAB_14000178b:
          param_1[7] = iVar8;
          return 0;
        }
        param_1[0x61] = param_1[0x61] + 1;
        if ((uVar18 != 0) && (iVar5 = iVar6 * param_5 * 3, 0 < iVar5)) {
          FUN_140007940(param_2,0,(undefined1 *)(longlong)iVar5);
        }
      }
      else {
        if (param_6 != 0x78c) {
          return 0xfffffff7;
        }
        uVar20 = (longlong)param_3 / (longlong)(int)(param_5 * 4);
        iVar6 = (int)uVar20;
        uVar20 = uVar20 & 0xffffffff;
        if (iVar6 <= iVar14) {
          if ((param_7 & 0x100000) == 0) {
            uVar23 = uVar11;
            if (0 < (int)uVar3) {
              do {
                puVar21 = (undefined4 *)((longlong)param_2 + (longlong)(int)uVar23 * 4);
                uVar25 = uVar20;
                iVar8 = iVar5;
                iVar14 = iVar5;
                if (0 < iVar6) {
                  do {
                    *puVar21 = *(undefined4 *)
                                ((longlong)param_1 +
                                ((longlong)(int)(iVar14 * uVar1) + uVar11) * 4 + (longlong)iVar4);
                    iVar8 = 0;
                    if (iVar14 + 1 < iVar2) {
                      iVar8 = iVar14 + 1;
                    }
                    puVar21 = puVar21 + uVar15;
                    uVar25 = uVar25 - 1;
                    iVar14 = iVar8;
                  } while (uVar25 != 0);
                }
                uVar11 = uVar11 + 1;
                uVar23 = (ulonglong)((int)uVar23 + 1);
              } while ((longlong)uVar11 < lVar17);
            }
          }
          else {
            uVar23 = uVar11;
            if (0 < (int)uVar3) {
              do {
                piVar16 = (int *)((longlong)param_2 + (longlong)(int)uVar23 * 4);
                uVar25 = uVar20;
                iVar8 = iVar5;
                iVar14 = iVar5;
                if (0 < iVar6) {
                  do {
                    *piVar16 = *piVar16 +
                               *(int *)((longlong)param_1 +
                                       ((longlong)(int)(iVar14 * uVar1) + uVar11) * 4 +
                                       (longlong)iVar4);
                    iVar8 = 0;
                    if (iVar14 + 1 < iVar2) {
                      iVar8 = iVar14 + 1;
                    }
                    piVar16 = piVar16 + uVar15;
                    uVar25 = uVar25 - 1;
                    iVar14 = iVar8;
                  } while (uVar25 != 0);
                }
                uVar11 = uVar11 + 1;
                uVar23 = (ulonglong)((int)uVar23 + 1);
              } while ((longlong)uVar11 < lVar17);
            }
          }
          goto LAB_14000178b;
        }
        param_1[0x61] = param_1[0x61] + 1;
        if ((uVar18 != 0) && (0 < (int)(iVar6 * param_5))) {
          FUN_140007940(param_2,0,(undefined1 *)((longlong)(int)(iVar6 * param_5) << 2));
        }
      }
      uVar15 = 0xfffffff9;
    }
    param_1[0x62] = 1;
    return uVar15;
  }
  uVar10 = 0;
  uVar24 = 0xfffffff4;
  if (iVar2 >> 1 <= iVar14) {
    uVar24 = uVar10;
  }
  if (param_6 == 8) {
    uVar10 = param_3 / (int)param_5;
  }
  else {
    if (param_6 == 0x10) {
      iVar8 = param_5 * 2;
    }
    else if (param_6 == 0x18) {
      iVar8 = param_5 * 3;
    }
    else {
      if (param_6 != 0x78c) goto LAB_140001354;
      iVar8 = param_5 * 4;
    }
    uVar10 = param_3 / iVar8;
  }
LAB_140001354:
  if ((int)uVar10 < iVar14) {
    if (iVar2 >> 1 <= iVar14) {
      param_1[0x62] = 0;
      goto LAB_1400013e4;
    }
  }
  else {
    uVar24 = 0xfffffff3;
  }
  if (uVar18 == 0) goto LAB_1400013d2;
  if (param_6 == 8) {
    iVar5 = uVar10 * param_5;
    if (iVar5 < 1) goto LAB_1400013d2;
    bVar9 = 0x80;
  }
  else if (param_6 == 0x10) {
    iVar5 = uVar10 * param_5 * 2;
    if (iVar5 < 1) goto LAB_1400013d2;
    bVar9 = 0;
  }
  else {
    if (param_6 != 0x18) {
      if ((param_6 == 0x78c) && (0 < (int)(uVar10 * param_5))) {
        FUN_140007940(param_2,0,(undefined1 *)((longlong)(int)(uVar10 * param_5) << 2));
      }
      goto LAB_1400013d2;
    }
    iVar5 = uVar10 * param_5 * 3;
    if (iVar5 < 1) goto LAB_1400013d2;
    bVar9 = 0;
  }
  FUN_140007940(param_2,bVar9,(undefined1 *)(longlong)iVar5);
LAB_1400013d2:
  return (ulonglong)uVar24;
}



// === FUN_1400017ac @ 1400017ac (size=2817) ===

/* WARNING: Function: __security_check_cookie replaced with injection: security_check_cookie */

longlong FUN_1400017ac(longlong param_1,longlong *param_2,int param_3,int param_4,uint param_5,
                      int param_6,int param_7)

{
  int *piVar1;
  int *piVar2;
  uint uVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  int iVar7;
  undefined1 *puVar8;
  longlong lVar9;
  byte bVar10;
  int iVar11;
  longlong lVar12;
  longlong lVar13;
  int iVar14;
  ulonglong uVar15;
  ulonglong uVar16;
  longlong *plVar17;
  longlong lVar18;
  int iVar19;
  undefined1 auStack_148 [32];
  int local_128;
  int local_124;
  int local_120;
  uint local_11c;
  int local_118;
  longlong local_110;
  longlong *local_108;
  uint local_100;
  uint local_f8;
  longlong local_e8;
  int local_d8 [16];
  int aiStack_98 [16];
  ulonglong local_58;
  
  local_58 = DAT_140012be0 ^ (ulonglong)auStack_148;
  uVar15 = (ulonglong)(int)param_5;
  local_11c = *(uint *)(param_1 + 0x10);
  local_118 = *(int *)(param_1 + 0x14);
  iVar14 = *(int *)(param_1 + 0x1c);
  local_f8 = param_5;
  if ((int)local_11c < (int)param_5) {
    local_f8 = local_11c;
  }
  lVar12 = 0;
  iVar11 = 0;
  iVar6 = 300;
  iVar4 = *(int *)(param_1 + 0x18) - iVar14;
  iVar19 = *(int *)(param_1 + 0x20);
  local_100 = param_5;
  local_124 = iVar4 + local_118;
  if (-1 < iVar4) {
    local_124 = iVar4;
  }
  local_e8 = *(int *)(param_1 + 8) + param_1;
  if ((((param_4 != (param_4 / 300) * 300) || (iVar19 != (iVar19 / 300) * 300)) &&
      ((iVar6 = 100, param_4 != (param_4 / 100) * 100 || (iVar19 != (iVar19 / 100) * 100)))) &&
     ((iVar6 = 0x4b, param_4 != (param_4 / 0x4b) * 0x4b || (iVar19 != (iVar19 / 0x4b) * 0x4b)))) {
    return 0xfffffe1a;
  }
  local_128 = param_4 / iVar6;
  iVar19 = iVar19 / iVar6;
  local_110 = param_1;
  local_120 = param_3;
  local_108 = param_2;
  if (*(int *)(param_1 + 0x188) == 0) {
LAB_1400019d1:
    lVar13 = (longlong)(int)local_f8;
    iVar4 = *(int *)(param_1 + 0xb8);
    if (0 < lVar13) {
      FUN_140007680((undefined8 *)local_d8,(undefined8 *)(param_1 + 0xbc),lVar13 << 2);
    }
    lVar18 = local_110;
    if (param_6 == 8) {
      lVar18 = (longlong)local_120;
      local_120 = (int)(lVar18 / (longlong)(int)param_5);
      uVar16 = lVar18 / (longlong)(int)param_5 & 0xffffffff;
      if (0 < local_120) {
        plVar17 = local_108;
        if (local_128 < iVar19) {
          do {
            iVar6 = local_128;
            if (iVar19 - iVar4 < local_128) {
              iVar6 = iVar19 - iVar4;
            }
            if (0 < lVar13) {
              lVar18 = lVar12;
              do {
                piVar1 = local_d8 + lVar18;
                iVar5 = *(int *)(((local_e8 + (longlong)(int)(iVar14 * local_11c) * 4) -
                                 (longlong)local_d8) + (longlong)piVar1);
                iVar7 = (iVar6 * iVar5) / local_128;
                *piVar1 = *piVar1 + iVar7;
                aiStack_98[lVar18] = iVar5 - iVar7;
                lVar18 = lVar18 + 1;
              } while (lVar18 < lVar13);
              uVar15 = (ulonglong)local_100;
            }
            iVar4 = iVar4 + iVar6;
            if (iVar4 == iVar19) {
              lVar18 = lVar12;
              if (0 < lVar13) {
                do {
                  *(char *)(lVar18 + (longlong)plVar17) =
                       (char)((local_128 * local_d8[lVar18]) / iVar19 >> 0xb) + -0x80;
                  local_d8[lVar18] = aiStack_98[lVar18];
                  lVar18 = lVar18 + 1;
                } while (lVar18 < lVar13);
              }
              iVar4 = local_128 - iVar6;
              plVar17 = (longlong *)((longlong)plVar17 + (longlong)(int)uVar15);
              uVar16 = (ulonglong)((int)uVar16 - 1);
            }
            iVar6 = iVar14 + 1;
            iVar14 = iVar11;
            if (iVar6 < local_118) {
              iVar14 = iVar6;
            }
            local_124 = local_124 + -1;
            if ((local_124 < 1) && (0 < (int)uVar16)) {
              *(int *)(local_110 + 0x184) = *(int *)(local_110 + 0x184) + 1;
              if ((param_7 != 0) && (iVar14 = (int)uVar15 * local_120, 0 < iVar14)) {
                FUN_140007940(local_108,0x80,(undefined1 *)(longlong)iVar14);
              }
              lVar12 = 0xfffffff5;
              goto LAB_140002278;
            }
          } while (0 < (int)uVar16);
        }
        else {
          do {
            for (; (iVar19 <= iVar4 && (0 < (int)uVar16)); uVar16 = (ulonglong)((int)uVar16 - 1)) {
              lVar18 = lVar12;
              if (0 < lVar13) {
                do {
                  *(char *)(lVar18 + (longlong)plVar17) = (char)(local_d8[lVar18] >> 0xb) + -0x80;
                  lVar18 = lVar18 + 1;
                } while (lVar18 < lVar13);
              }
              iVar4 = iVar4 - iVar19;
              plVar17 = (longlong *)((longlong)plVar17 + uVar15);
            }
            lVar18 = lVar12;
            if (0 < lVar13) {
              do {
                aiStack_98[lVar18] =
                     *(int *)(local_e8 + (longlong)(int)(iVar14 * local_11c) * 4 + lVar18 * 4);
                lVar18 = lVar18 + 1;
              } while (lVar18 < lVar13);
            }
            iVar6 = local_128;
            if (iVar4 < 1) goto LAB_1400021f7;
            lVar18 = lVar12;
            if (0 < lVar13) {
              do {
                *(char *)(lVar18 + (longlong)plVar17) =
                     (char)(((iVar19 - iVar4) * aiStack_98[lVar18] + iVar4 * local_d8[lVar18]) /
                            iVar19 >> 0xb) + -0x80;
                lVar18 = lVar18 + 1;
              } while (lVar18 < lVar13);
            }
            iVar6 = local_128 - (iVar19 - iVar4);
            while( true ) {
              plVar17 = (longlong *)((longlong)plVar17 + uVar15);
              uVar16 = (ulonglong)((int)uVar16 - 1);
LAB_1400021f7:
              iVar4 = iVar6;
              iVar6 = (int)uVar16;
              if ((iVar4 < iVar19) || (iVar6 < 1)) break;
              lVar18 = lVar12;
              if (0 < lVar13) {
                do {
                  *(char *)(lVar18 + (longlong)plVar17) = (char)(aiStack_98[lVar18] >> 0xb) + -0x80;
                  lVar18 = lVar18 + 1;
                } while (lVar18 < lVar13);
              }
              iVar6 = iVar4 - iVar19;
            }
            lVar18 = lVar12;
            if (0 < lVar13) {
              do {
                local_d8[lVar18] = aiStack_98[lVar18];
                lVar18 = lVar18 + 1;
              } while (lVar18 < lVar13);
            }
            iVar5 = iVar14 + 1;
            iVar14 = iVar11;
            if (iVar5 < local_118) {
              iVar14 = iVar5;
            }
            local_124 = local_124 + -1;
            if ((local_124 < 1) && (0 < iVar6)) {
              *(int *)(local_110 + 0x184) = *(int *)(local_110 + 0x184) + 1;
              if ((param_7 != 0) && (0 < (int)(param_5 * local_120))) {
                FUN_140007940(local_108,0x80,(undefined1 *)(longlong)(int)(param_5 * local_120));
              }
              lVar12 = 0xfffffff4;
              goto LAB_140002278;
            }
          } while (0 < iVar6);
        }
      }
    }
    else if (param_6 == 0x10) {
      uVar16 = (longlong)local_120 / (longlong)(int)(param_5 * 2);
      local_120 = (int)uVar16;
      uVar16 = uVar16 & 0xffffffff;
      if (0 < local_120) {
        plVar17 = local_108;
        if (local_128 < iVar19) {
          do {
            iVar6 = local_128;
            if (iVar19 - iVar4 < local_128) {
              iVar6 = iVar19 - iVar4;
            }
            if (0 < lVar13) {
              lVar18 = lVar12;
              do {
                piVar1 = local_d8 + lVar18;
                iVar5 = *(int *)(((local_e8 + (longlong)(int)(iVar14 * local_11c) * 4) -
                                 (longlong)local_d8) + (longlong)piVar1);
                iVar7 = (iVar6 * iVar5) / local_128;
                *piVar1 = *piVar1 + iVar7;
                aiStack_98[lVar18] = iVar5 - iVar7;
                lVar18 = lVar18 + 1;
              } while (lVar18 < lVar13);
              uVar15 = (ulonglong)local_100;
            }
            iVar4 = iVar4 + iVar6;
            if (iVar4 == iVar19) {
              lVar18 = lVar12;
              if (0 < lVar13) {
                do {
                  *(short *)((longlong)plVar17 + lVar18 * 2) =
                       (short)((local_128 * local_d8[lVar18]) / iVar19 >> 3);
                  local_d8[lVar18] = aiStack_98[lVar18];
                  lVar18 = lVar18 + 1;
                } while (lVar18 < lVar13);
              }
              iVar4 = local_128 - iVar6;
              uVar16 = (ulonglong)((int)uVar16 - 1);
              plVar17 = (longlong *)((longlong)plVar17 + (longlong)(int)uVar15 * 2);
            }
            iVar6 = iVar14 + 1;
            iVar14 = iVar11;
            if (iVar6 < local_118) {
              iVar14 = iVar6;
            }
            local_124 = local_124 + -1;
            if ((local_124 < 1) && (0 < (int)uVar16)) {
              *(int *)(local_110 + 0x184) = *(int *)(local_110 + 0x184) + 1;
              if ((param_7 != 0) && (iVar14 = (int)uVar15 * local_120 * 2, 0 < iVar14)) {
                FUN_140007940(local_108,0,(undefined1 *)(longlong)iVar14);
              }
              lVar12 = 0xfffffff3;
              goto LAB_140002278;
            }
          } while (0 < (int)uVar16);
        }
        else {
          do {
            for (; (iVar19 <= iVar4 && (0 < (int)uVar16)); uVar16 = (ulonglong)((int)uVar16 - 1)) {
              lVar9 = lVar12;
              if (0 < lVar13) {
                do {
                  *(short *)((longlong)plVar17 + lVar9 * 2) = (short)(local_d8[lVar9] >> 3);
                  lVar9 = lVar9 + 1;
                } while (lVar9 < lVar13);
              }
              iVar4 = iVar4 - iVar19;
              plVar17 = (longlong *)((longlong)plVar17 + uVar15 * 2);
            }
            lVar9 = lVar12;
            if (0 < lVar13) {
              do {
                aiStack_98[lVar9] =
                     *(int *)(local_e8 + (longlong)(int)(iVar14 * local_11c) * 4 + lVar9 * 4);
                lVar9 = lVar9 + 1;
              } while (lVar9 < lVar13);
            }
            iVar6 = local_128;
            if (iVar4 < 1) goto LAB_140001f63;
            lVar9 = lVar12;
            if (0 < lVar13) {
              do {
                *(short *)((longlong)plVar17 + lVar9 * 2) =
                     (short)(((iVar19 - iVar4) * aiStack_98[lVar9] + iVar4 * local_d8[lVar9]) /
                             iVar19 >> 3);
                lVar9 = lVar9 + 1;
              } while (lVar9 < lVar13);
            }
            iVar6 = local_128 - (iVar19 - iVar4);
            while( true ) {
              plVar17 = (longlong *)((longlong)plVar17 + uVar15 * 2);
              uVar16 = (ulonglong)((int)uVar16 - 1);
LAB_140001f63:
              iVar4 = iVar6;
              iVar6 = (int)uVar16;
              if ((iVar4 < iVar19) || (iVar6 < 1)) break;
              lVar9 = lVar12;
              if (0 < lVar13) {
                do {
                  *(short *)((longlong)plVar17 + lVar9 * 2) = (short)(aiStack_98[lVar9] >> 3);
                  lVar9 = lVar9 + 1;
                } while (lVar9 < lVar13);
              }
              iVar6 = iVar4 - iVar19;
            }
            lVar9 = lVar12;
            if (0 < lVar13) {
              do {
                local_d8[lVar9] = aiStack_98[lVar9];
                lVar9 = lVar9 + 1;
              } while (lVar9 < lVar13);
            }
            iVar5 = iVar14 + 1;
            iVar14 = iVar11;
            if (iVar5 < local_118) {
              iVar14 = iVar5;
            }
            local_124 = local_124 + -1;
            if ((local_124 < 1) && (0 < iVar6)) {
              *(int *)(local_110 + 0x184) = *(int *)(local_110 + 0x184) + 1;
              if ((param_7 != 0) && (iVar14 = param_5 * local_120 * 2, 0 < iVar14)) {
                FUN_140007940(local_108,0,(undefined1 *)(longlong)iVar14);
              }
              lVar12 = 0xfffffff2;
              local_110 = lVar18;
              goto LAB_140002278;
            }
          } while (0 < iVar6);
        }
      }
    }
    else {
      if (param_6 != 0x18) {
        return 0xfffffff7;
      }
      uVar16 = (longlong)local_120 / (longlong)(int)(param_5 * 3);
      local_120 = (int)uVar16;
      uVar16 = uVar16 & 0xffffffff;
      if (0 < local_120) {
        plVar17 = local_108;
        if (local_128 < iVar19) {
          do {
            iVar11 = local_128;
            if (iVar19 - iVar4 < local_128) {
              iVar11 = iVar19 - iVar4;
            }
            if (0 < lVar13) {
              lVar18 = lVar12;
              do {
                piVar1 = local_d8 + lVar18;
                iVar6 = *(int *)(((local_e8 + (longlong)(int)(iVar14 * local_11c) * 4) -
                                 (longlong)local_d8) + (longlong)piVar1);
                iVar5 = (iVar11 * iVar6) / local_128;
                *piVar1 = *piVar1 + iVar5;
                aiStack_98[lVar18] = iVar6 - iVar5;
                lVar18 = lVar18 + 1;
              } while (lVar18 < lVar13);
              uVar15 = (ulonglong)local_100;
            }
            iVar4 = iVar4 + iVar11;
            if (iVar4 == iVar19) {
              if (0 < lVar13) {
                puVar8 = (undefined1 *)((longlong)plVar17 + 2);
                lVar18 = lVar12;
                do {
                  iVar4 = (local_128 * local_d8[lVar18]) / iVar19 << 5;
                  puVar8[-2] = (char)iVar4;
                  puVar8[-1] = (char)((uint)iVar4 >> 8);
                  *puVar8 = (char)((uint)iVar4 >> 0x10);
                  puVar8 = puVar8 + 3;
                  local_d8[lVar18] = aiStack_98[lVar18];
                  lVar18 = lVar18 + 1;
                } while (lVar18 < lVar13);
              }
              iVar4 = local_128 - iVar11;
              plVar17 = (longlong *)((longlong)plVar17 + (longlong)((int)uVar15 * 3));
              uVar16 = (ulonglong)((int)uVar16 - 1);
            }
            iVar11 = iVar14 + 1;
            iVar14 = 0;
            if (iVar11 < local_118) {
              iVar14 = iVar11;
            }
            local_124 = local_124 + -1;
            if ((local_124 < 1) && (0 < (int)uVar16)) {
              *(int *)(local_110 + 0x184) = *(int *)(local_110 + 0x184) + 1;
              if ((param_7 != 0) && (iVar14 = (int)uVar15 * local_120 * 3, 0 < iVar14)) {
                FUN_140007940(local_108,0,(undefined1 *)(longlong)iVar14);
              }
              lVar12 = 0xfffffff1;
              goto LAB_140002278;
            }
          } while (0 < (int)uVar16);
        }
        else {
          do {
            for (; (iVar19 <= iVar4 && (0 < (int)uVar16)); uVar16 = (ulonglong)((int)uVar16 - 1)) {
              if (0 < lVar13) {
                puVar8 = (undefined1 *)((longlong)plVar17 + 2);
                lVar18 = lVar12;
                do {
                  piVar1 = local_d8 + lVar18;
                  lVar18 = lVar18 + 1;
                  iVar6 = *piVar1 << 5;
                  puVar8[-2] = (char)iVar6;
                  puVar8[-1] = (char)((uint)iVar6 >> 8);
                  *puVar8 = (char)((uint)iVar6 >> 0x10);
                  puVar8 = puVar8 + 3;
                } while (lVar18 < lVar13);
              }
              iVar4 = iVar4 - iVar19;
              plVar17 = (longlong *)((longlong)plVar17 + (longlong)(int)(param_5 * 3));
            }
            lVar18 = lVar12;
            if (0 < lVar13) {
              do {
                aiStack_98[lVar18] =
                     *(int *)(local_e8 + (longlong)(int)(iVar14 * local_11c) * 4 + lVar18 * 4);
                lVar18 = lVar18 + 1;
              } while (lVar18 < lVar13);
            }
            iVar6 = local_128;
            if (iVar4 < 1) goto LAB_140001cca;
            if (0 < lVar13) {
              puVar8 = (undefined1 *)((longlong)plVar17 + 2);
              lVar18 = lVar12;
              do {
                piVar1 = aiStack_98 + lVar18;
                piVar2 = local_d8 + lVar18;
                lVar18 = lVar18 + 1;
                iVar6 = ((iVar19 - iVar4) * *piVar1 + iVar4 * *piVar2) / iVar19 << 5;
                puVar8[-2] = (char)iVar6;
                puVar8[-1] = (char)((uint)iVar6 >> 8);
                *puVar8 = (char)((uint)iVar6 >> 0x10);
                puVar8 = puVar8 + 3;
              } while (lVar18 < lVar13);
            }
            iVar6 = local_128 - (iVar19 - iVar4);
            while( true ) {
              plVar17 = (longlong *)((longlong)plVar17 + (longlong)(int)(param_5 * 3));
              uVar16 = (ulonglong)((int)uVar16 - 1);
LAB_140001cca:
              iVar4 = iVar6;
              iVar6 = (int)uVar16;
              if ((iVar4 < iVar19) || (iVar6 < 1)) break;
              if (0 < lVar13) {
                puVar8 = (undefined1 *)((longlong)plVar17 + 2);
                lVar18 = lVar12;
                do {
                  piVar1 = aiStack_98 + lVar18;
                  lVar18 = lVar18 + 1;
                  iVar6 = *piVar1 << 5;
                  puVar8[-2] = (char)iVar6;
                  puVar8[-1] = (char)((uint)iVar6 >> 8);
                  *puVar8 = (char)((uint)iVar6 >> 0x10);
                  puVar8 = puVar8 + 3;
                } while (lVar18 < lVar13);
              }
              iVar6 = iVar4 - iVar19;
            }
            lVar18 = lVar12;
            if (0 < lVar13) {
              do {
                local_d8[lVar18] = aiStack_98[lVar18];
                lVar18 = lVar18 + 1;
              } while (lVar18 < lVar13);
            }
            iVar5 = iVar14 + 1;
            iVar14 = iVar11;
            if (iVar5 < local_118) {
              iVar14 = iVar5;
            }
            local_124 = local_124 + -1;
            if ((local_124 < 1) && (0 < iVar6)) {
              *(int *)(local_110 + 0x184) = *(int *)(local_110 + 0x184) + 1;
              if ((param_7 != 0) && (iVar14 = param_5 * local_120 * 3, 0 < iVar14)) {
                FUN_140007940(local_108,0,(undefined1 *)(longlong)iVar14);
              }
              lVar12 = 0xfffffff0;
LAB_140002278:
              *(undefined4 *)(local_110 + 0x188) = 1;
              return lVar12;
            }
          } while (0 < iVar6);
        }
      }
    }
    *(int *)(local_110 + 0xb8) = iVar4;
    if (0 < lVar13) {
      do {
        *(int *)(local_110 + 0xbc + lVar12 * 4) = local_d8[lVar12];
        lVar12 = lVar12 + 1;
      } while (lVar12 < lVar13);
    }
    *(int *)(local_110 + 0x1c) = iVar14;
    lVar13 = 0;
  }
  else {
    lVar13 = 0xfffffff4;
    if (local_118 >> 1 <= local_124) {
      lVar13 = lVar12;
    }
    uVar3 = param_5;
    if (param_6 == 8) {
LAB_140001956:
      iVar4 = param_3 / (int)uVar3;
    }
    else {
      if (param_6 == 0x10) {
        uVar3 = param_5 * 2;
        goto LAB_140001956;
      }
      iVar4 = iVar11;
      if (param_6 == 0x18) {
        uVar3 = param_5 * 3;
        goto LAB_140001956;
      }
    }
    if (local_124 < (iVar4 * iVar19) / local_128 + 1) {
      lVar13 = 0xfffffff3;
    }
    else if (local_118 >> 1 <= local_124) {
      *(undefined4 *)(param_1 + 0x188) = 0;
      goto LAB_1400019d1;
    }
    if (param_7 != 0) {
      if (param_6 == 8) {
        iVar4 = param_5 * iVar4;
        if (iVar4 < 1) {
          return lVar13;
        }
        bVar10 = 0x80;
      }
      else if (param_6 == 0x10) {
        iVar4 = param_5 * iVar4 * 2;
        if (iVar4 < 1) {
          return lVar13;
        }
        bVar10 = 0;
      }
      else {
        if (param_6 != 0x18) {
          return lVar13;
        }
        iVar4 = param_5 * iVar4 * 3;
        if (iVar4 < 1) {
          return lVar13;
        }
        bVar10 = 0;
      }
      FUN_140007940(param_2,bVar10,(undefined1 *)(longlong)iVar4);
    }
  }
  return lVar13;
}



// === FUN_1400022b0 @ 1400022b0 (size=1007) ===

undefined8
FUN_1400022b0(int *param_1,undefined8 *param_2,int param_3,int param_4,uint param_5,int param_6,
             int param_7)

{
  uint uVar1;
  int iVar2;
  int iVar3;
  uint uVar4;
  int iVar5;
  undefined8 uVar6;
  undefined8 *puVar7;
  longlong lVar8;
  undefined2 *puVar9;
  int iVar10;
  longlong lVar11;
  ulonglong uVar12;
  ulonglong uVar13;
  int iVar14;
  longlong lVar15;
  ulonglong uVar16;
  uint uVar17;
  longlong lVar18;
  
  if (param_1[2] == 0) {
    return 0xffffffff;
  }
  if (param_3 == 0) {
    return 0xfffffffe;
  }
  lVar11 = (longlong)(int)param_5;
  if ((int)param_5 < 1) {
    return 0xfffffffd;
  }
  if (param_4 < 8000) {
    return 0xfffffffc;
  }
  if (200000 < param_4) {
    return 0xfffffffb;
  }
  FUN_140001180(param_1,1);
  if ((0 < param_1[8]) && (param_1[8] != param_4)) {
    uVar6 = FUN_1400026a0((longlong)param_1,(longlong)param_2,param_3,param_4,param_5,param_6,
                          param_7);
    return uVar6;
  }
  uVar1 = param_1[4];
  iVar2 = param_1[5];
  iVar3 = param_1[2];
  iVar10 = param_1[6];
  uVar4 = param_5;
  if ((int)uVar1 < (int)param_5) {
    uVar4 = uVar1;
  }
  iVar5 = param_1[7] - iVar10;
  lVar18 = (longlong)(int)uVar4;
  iVar14 = iVar5 + iVar2;
  if (0 < iVar5) {
    iVar14 = iVar5;
  }
  iVar14 = iVar14 + -2;
  if (param_6 == 8) {
    uVar16 = (longlong)param_3 / (longlong)(int)param_5;
    uVar17 = (uint)uVar16;
    uVar13 = uVar16 & 0xffffffff;
    uVar16 = uVar16 & 0xffffffff;
    if (iVar14 <= (int)uVar17) {
      uVar6 = 0xfffffffb;
      goto LAB_1400025c6;
    }
    while (0 < (int)uVar17) {
      lVar8 = 0;
      if (0 < lVar18) {
        do {
          *(uint *)((longlong)param_1 +
                   lVar8 * 4 + (longlong)(int)(iVar10 * uVar1) * 4 + (longlong)iVar3) =
               (*(byte *)(lVar8 + (longlong)param_2) - 0x80) * 0x800;
          lVar8 = lVar8 + 1;
        } while (lVar8 < lVar18);
      }
      iVar14 = iVar10 + 1;
      iVar10 = 0;
      if (iVar14 < iVar2) {
        iVar10 = iVar14;
      }
      param_2 = (undefined8 *)((longlong)param_2 + lVar11);
      uVar17 = (int)uVar13 - 1;
      uVar13 = (ulonglong)uVar17;
    }
  }
  else {
    if (param_6 != 0x10) {
      if (param_6 == 0x18) {
        uVar13 = (longlong)param_3 / (longlong)(int)(param_5 * 3);
        uVar12 = uVar13 & 0xffffffff;
        uVar16 = uVar13 & 0xffffffff;
        if ((int)uVar13 < iVar14) {
          if (0 < (int)uVar13) {
            do {
              lVar11 = 0;
              if (0 < lVar18) {
                puVar9 = (undefined2 *)((longlong)param_2 + 1);
                do {
                  *(int *)((longlong)param_1 +
                          lVar11 * 4 + (longlong)(int)(iVar10 * uVar1) * 4 + (longlong)iVar3) =
                       (int)((uint)CONCAT21(*puVar9,*(undefined1 *)((longlong)puVar9 + -1)) << 8) >>
                       0xd;
                  lVar11 = lVar11 + 1;
                  puVar9 = (undefined2 *)((longlong)puVar9 + 3);
                } while (lVar11 < lVar18);
              }
              iVar14 = iVar10 + 1;
              iVar10 = 0;
              if (iVar14 < iVar2) {
                iVar10 = iVar14;
              }
              param_2 = (undefined8 *)((longlong)param_2 + (longlong)(int)(param_5 * 3));
              uVar17 = (int)uVar12 - 1;
              uVar12 = (ulonglong)uVar17;
            } while (0 < (int)uVar17);
          }
          goto LAB_140002619;
        }
      }
      else {
        if (param_6 != 0x78c) {
          return 0xfffffff7;
        }
        uVar13 = (longlong)param_3 / (longlong)(int)(param_5 * 4);
        uVar12 = uVar13 & 0xffffffff;
        uVar16 = uVar13 & 0xffffffff;
        if ((int)uVar13 < iVar14) {
          if (0 < (int)uVar13) {
            do {
              puVar7 = (undefined8 *)
                       ((longlong)param_1 + (longlong)(int)(iVar10 * uVar1) * 4 + (longlong)iVar3);
              if ((((int)uVar4 < 1) || (uVar4 < 4)) ||
                 ((puVar7 <= (undefined8 *)((longlong)param_2 + (longlong)(int)(uVar4 - 1) * 4) &&
                  (param_2 <= (undefined8 *)((longlong)puVar7 + (longlong)(int)(uVar4 - 1) * 4)))))
              {
                if (0 < lVar18) {
                  lVar15 = (longlong)param_2 - (longlong)puVar7;
                  lVar8 = lVar18;
                  do {
                    *(undefined4 *)puVar7 = *(undefined4 *)(lVar15 + (longlong)puVar7);
                    puVar7 = (undefined8 *)((longlong)puVar7 + 4);
                    lVar8 = lVar8 + -1;
                  } while (lVar8 != 0);
                }
              }
              else {
                FUN_140007680(puVar7,param_2,lVar18 * 4);
              }
              iVar14 = iVar10 + 1;
              iVar10 = 0;
              if (iVar14 < iVar2) {
                iVar10 = iVar14;
              }
              param_2 = (undefined8 *)((longlong)param_2 + lVar11 * 4);
              uVar17 = (int)uVar12 - 1;
              uVar12 = (ulonglong)uVar17;
            } while (0 < (int)uVar17);
          }
          goto LAB_140002619;
        }
      }
      uVar6 = 0xfffffff9;
LAB_1400025c6:
      param_1[0x60] = param_1[0x60] + 1;
      return uVar6;
    }
    uVar13 = (longlong)param_3 / (longlong)(int)(param_5 * 2);
    uVar12 = uVar13 & 0xffffffff;
    uVar16 = uVar13 & 0xffffffff;
    if (iVar14 <= (int)uVar13) {
      uVar6 = 0xfffffffa;
      goto LAB_1400025c6;
    }
    if (0 < (int)uVar13) {
      do {
        lVar8 = 0;
        if (0 < lVar18) {
          do {
            *(int *)((longlong)param_1 +
                    lVar8 * 4 + (longlong)(int)(iVar10 * uVar1) * 4 + (longlong)iVar3) =
                 (int)*(short *)((longlong)param_2 + lVar8 * 2) << 3;
            lVar8 = lVar8 + 1;
          } while (lVar8 < lVar18);
        }
        iVar14 = iVar10 + 1;
        iVar10 = 0;
        if (iVar14 < iVar2) {
          iVar10 = iVar14;
        }
        param_2 = (undefined8 *)((longlong)param_2 + lVar11 * 2);
        uVar17 = (int)uVar12 - 1;
        uVar12 = (ulonglong)uVar17;
      } while (0 < (int)uVar17);
    }
  }
LAB_140002619:
  if (((param_7 != 0) && ((int)uVar4 < (int)uVar1)) && (iVar10 = param_1[6], 0 < (int)uVar16)) {
    do {
      FUN_140007940((longlong *)
                    ((longlong)param_1 + ((int)(iVar10 * uVar1) + lVar18) * 4 + (longlong)iVar3),0,
                    (undefined1 *)((longlong)(int)(uVar1 - uVar4) << 2));
      iVar14 = iVar10 + 1;
      iVar10 = 0;
      if (iVar14 < iVar2) {
        iVar10 = iVar14;
      }
      uVar17 = (int)uVar16 - 1;
      uVar16 = (ulonglong)uVar17;
    } while (0 < (int)uVar17);
  }
  param_1[6] = iVar10;
  return 0;
}



// === FUN_1400026a0 @ 1400026a0 (size=4808) ===

/* WARNING: Function: __security_check_cookie replaced with injection: security_check_cookie */

undefined8
FUN_1400026a0(longlong param_1,longlong param_2,int param_3,int param_4,uint param_5,int param_6,
             int param_7)

{
  undefined4 uVar1;
  int iVar2;
  sbyte sVar3;
  int iVar4;
  int iVar5;
  uint uVar6;
  int *piVar7;
  undefined4 *puVar8;
  ulonglong uVar9;
  undefined8 *puVar10;
  undefined2 *puVar11;
  longlong lVar12;
  longlong lVar13;
  longlong lVar14;
  undefined4 *puVar15;
  int iVar16;
  uint uVar17;
  uint uVar18;
  int iVar19;
  ulonglong uVar20;
  ulonglong uVar21;
  longlong lVar22;
  int iVar23;
  int iVar24;
  undefined8 *puVar25;
  undefined1 auStack_158 [32];
  uint local_138;
  int local_134;
  uint local_130;
  longlong local_128;
  int local_120;
  longlong local_118;
  int local_110;
  longlong local_108;
  int local_100;
  int local_fc;
  int local_f8;
  longlong local_f0;
  uint local_e8;
  int local_e4;
  int *local_e0;
  longlong local_d8;
  int local_c8 [16];
  int local_88 [16];
  ulonglong local_48;
  
  local_48 = DAT_140012be0 ^ (ulonglong)auStack_158;
  lVar22 = (longlong)(int)param_5;
  local_118 = CONCAT44(local_118._4_4_,*(int *)(param_1 + 0x24));
  if (*(int *)(param_1 + 0x24) == 0) {
    return 0xffffffff;
  }
  local_108 = *(int *)(param_1 + 8) + param_1;
  local_e4 = *(int *)(param_1 + 0x20);
  local_138 = *(uint *)(param_1 + 0x10);
  local_e8 = param_5;
  if ((int)local_138 < (int)param_5) {
    local_e8 = local_138;
  }
  local_e0 = (int *)(param_1 + 0x18);
  iVar19 = *local_e0;
  iVar2 = *(int *)(param_1 + 0x1c) - iVar19;
  local_134 = *(int *)(param_1 + 0x14);
  iVar23 = iVar2 + local_134;
  if (0 < iVar2) {
    iVar23 = iVar2;
  }
  uVar17 = param_5;
  if (param_6 != 8) {
    if (param_6 == 0x10) {
      uVar17 = param_5 * 2;
    }
    else {
      if (param_6 != 0x18) {
        return 0xfffffffe;
      }
      uVar17 = param_5 * 3;
    }
  }
  iVar2 = param_3 / (int)uVar17;
  iVar16 = 300;
  iVar24 = 300;
  local_f8 = 300;
  local_fc = param_4 % 300;
  if ((local_fc != 0) || (local_e4 != (local_e4 / 300) * 300)) {
    iVar24 = 100;
    local_f8 = 100;
    if ((param_4 != (param_4 / 100) * 100) || (local_e4 != (local_e4 / 100) * 100)) {
      iVar24 = 0x4b;
      local_f8 = 0x4b;
      if (param_4 != (param_4 / 0x4b) * 0x4b) {
        return 0xfffffe1a;
      }
      if (local_e4 != (local_e4 / 0x4b) * 0x4b) {
        return 0xfffffe1a;
      }
    }
  }
  local_130 = local_e4 / iVar24;
  if (iVar23 + -2 < (int)(local_130 * iVar2) / (param_4 / iVar24) + 1) {
    *(int *)(param_1 + 0x180) = *(int *)(param_1 + 0x180) + 1;
    return 0xfffffffd;
  }
  iVar4 = 0;
  iVar24 = 0;
  iVar23 = iVar4;
  local_128 = param_2;
  local_120 = iVar19;
  local_110 = param_4;
  local_100 = param_3;
  local_f0 = param_1;
  if (param_4 < 0xac44) {
    iVar23 = 2;
    if (local_e4 >> 1 < param_4) {
      iVar23 = iVar4;
    }
    if (param_4 <= local_e4 >> 2) {
      iVar23 = 4;
    }
    if (param_4 <= local_e4 >> 3) {
      iVar23 = 8;
    }
    if (local_e4 >> 4 < param_4) goto LAB_140002965;
    iVar23 = 0x10;
  }
  else {
LAB_140002965:
    lVar14 = (longlong)(int)local_e8;
    if (iVar23 == 0) {
      if ((((local_fc != 0) || (local_e4 != (local_e4 / 300) * 300)) &&
          ((iVar16 = 100, param_4 != (param_4 / 100) * 100 || (local_e4 != (local_e4 / 100) * 100)))
          ) && ((iVar16 = 0x4b, param_4 != (param_4 / 0x4b) * 0x4b ||
                (local_e4 != (local_e4 / 0x4b) * 0x4b)))) {
        return 0xfffffe1a;
      }
      iVar2 = param_4 / iVar16;
      uVar17 = (uint)((longlong)local_e4 / (longlong)iVar16);
      uVar9 = (longlong)local_e4 / (longlong)iVar16 & 0xffffffff;
      iVar23 = *(int *)(param_1 + 0x34);
      local_130 = uVar17;
      local_d8 = lVar14;
      if (0 < lVar14) {
        FUN_140007680((undefined8 *)local_c8,(undefined8 *)(param_1 + 0x38),lVar14 << 2);
      }
      if (param_6 == 8) {
        uVar21 = (longlong)local_100 / (longlong)(int)param_5 & 0xffffffff;
        if (0 < (int)((longlong)local_100 / (longlong)(int)param_5)) {
          iVar16 = iVar23;
          if ((int)uVar17 < iVar2) {
            do {
              lVar13 = 0;
              uVar18 = uVar17;
              if (iVar2 - iVar23 < (int)uVar17) {
                uVar18 = iVar2 - iVar23;
              }
              if (0 < lVar14) {
                do {
                  iVar16 = (*(byte *)(lVar13 + local_128) - 0x80) * 0x800;
                  iVar24 = (int)(uVar18 * iVar16) / (int)uVar17;
                  local_c8[lVar13] = local_c8[lVar13] + iVar24;
                  local_88[lVar13] = iVar16 - iVar24;
                  lVar13 = lVar13 + 1;
                } while (lVar13 < lVar14);
              }
              lVar13 = 0;
              iVar23 = iVar23 + uVar18;
              iVar24 = iVar4;
              if (iVar23 == iVar2) {
                if (0 < lVar14) {
                  do {
                    *(int *)((longlong)(local_88 + lVar13) +
                            ((local_108 + (longlong)(int)(iVar19 * local_138) * 4) -
                            (longlong)local_88)) = (int)(uVar17 * local_c8[lVar13]) / iVar2;
                    local_c8[lVar13] = local_88[lVar13];
                    lVar13 = lVar13 + 1;
                  } while (lVar13 < lVar14);
                }
                iVar19 = iVar19 + 1;
                iVar23 = uVar17 - uVar18;
                iVar24 = iVar4 + 1;
                if (local_134 <= iVar19) {
                  iVar19 = 0;
                }
              }
              local_128 = local_128 + lVar22;
              uVar18 = (int)uVar21 - 1;
              uVar21 = (ulonglong)uVar18;
              iVar4 = iVar24;
              local_120 = iVar19;
              local_118 = lVar22;
            } while (0 < (int)uVar18);
          }
          else {
            do {
              lVar12 = 0;
              lVar13 = lVar12;
              if (0 < lVar14) {
                do {
                  local_88[lVar13] = (*(byte *)(lVar13 + local_128) - 0x80) * 0x800;
                  lVar13 = lVar13 + 1;
                } while (lVar13 < lVar14);
                uVar9 = (ulonglong)local_130;
              }
              iVar23 = iVar16 + (int)uVar9;
              iVar24 = iVar4;
              if (0 < iVar16) {
                if (0 < lVar14) {
                  lVar13 = local_108 + (longlong)(int)(iVar19 * local_138) * 4;
                  do {
                    piVar7 = (int *)(lVar12 * 4 + lVar13);
                    iVar24 = *(int *)((longlong)piVar7 + ((longlong)local_88 - lVar13));
                    iVar5 = local_c8[lVar12];
                    local_c8[lVar12] = iVar24;
                    lVar12 = lVar12 + 1;
                    *piVar7 = (iVar24 * (iVar2 - iVar16)) / iVar2 + iVar5;
                  } while (lVar12 < lVar14);
                  uVar9 = (ulonglong)local_130;
                }
                iVar19 = iVar19 + 1;
                iVar23 = iVar23 - iVar2;
                iVar24 = iVar4 + 1;
                if (local_134 <= iVar19) {
                  iVar19 = 0;
                }
              }
              if (iVar2 <= iVar23) {
                do {
                  lVar13 = 0;
                  puVar15 = (undefined4 *)(local_108 + (longlong)(int)(iVar19 * local_138) * 4);
                  if (0 < lVar14) {
                    lVar12 = (longlong)local_88 - (longlong)puVar15;
                    do {
                      lVar13 = lVar13 + 1;
                      *puVar15 = *(undefined4 *)((longlong)puVar15 + lVar12);
                      puVar15 = puVar15 + 1;
                    } while (lVar13 < lVar14);
                  }
                  iVar16 = iVar19 + 1;
                  iVar23 = iVar23 - iVar2;
                  iVar24 = iVar24 + 1;
                  iVar19 = 0;
                  if (iVar16 < local_134) {
                    iVar19 = iVar16;
                  }
                } while (iVar2 <= iVar23);
                uVar9 = (ulonglong)local_130;
              }
              lVar13 = 0;
              if (0 < lVar14) {
                do {
                  local_c8[lVar13] = (iVar23 * local_88[lVar13]) / iVar2;
                  lVar13 = lVar13 + 1;
                } while (lVar13 < lVar14);
              }
              local_128 = local_128 + lVar22;
              uVar17 = (int)uVar21 - 1;
              uVar21 = (ulonglong)uVar17;
              iVar4 = iVar24;
              local_120 = iVar19;
              local_118 = lVar22;
              iVar16 = iVar23;
            } while (0 < (int)uVar17);
          }
        }
      }
      else if (param_6 == 0x10) {
        uVar21 = (longlong)local_100 / (longlong)(int)(param_5 * 2);
        uVar20 = uVar21 & 0xffffffff;
        if (0 < (int)uVar21) {
          if ((int)uVar17 < iVar2) {
            local_118 = lVar22 * 2;
            iVar24 = iVar4;
            do {
              lVar22 = 0;
              uVar18 = uVar17;
              if (iVar2 - iVar23 < (int)uVar17) {
                uVar18 = iVar2 - iVar23;
              }
              if (0 < lVar14) {
                do {
                  iVar4 = *(short *)(local_128 + lVar22 * 2) * 8;
                  iVar16 = (int)(uVar18 * iVar4) / (int)uVar17;
                  local_c8[lVar22] = local_c8[lVar22] + iVar16;
                  local_88[lVar22] = iVar4 - iVar16;
                  lVar22 = lVar22 + 1;
                } while (lVar22 < lVar14);
              }
              lVar22 = 0;
              iVar23 = iVar23 + uVar18;
              if (iVar23 == iVar2) {
                if (0 < lVar14) {
                  do {
                    *(int *)((longlong)(local_88 + lVar22) +
                            ((local_108 + (longlong)(int)(iVar19 * local_138) * 4) -
                            (longlong)local_88)) = (int)(uVar17 * local_c8[lVar22]) / iVar2;
                    local_c8[lVar22] = local_88[lVar22];
                    lVar22 = lVar22 + 1;
                  } while (lVar22 < lVar14);
                }
                iVar19 = iVar19 + 1;
                iVar23 = uVar17 - uVar18;
                iVar24 = iVar24 + 1;
                if (local_134 <= iVar19) {
                  iVar19 = 0;
                }
              }
              local_128 = local_128 + local_118;
              uVar18 = (int)uVar20 - 1;
              uVar20 = (ulonglong)uVar18;
              local_120 = iVar19;
            } while (0 < (int)uVar18);
          }
          else {
            local_118 = lVar22 * 2;
            iVar24 = iVar4;
            iVar16 = iVar23;
            do {
              lVar13 = 0;
              lVar22 = lVar13;
              if (0 < lVar14) {
                do {
                  local_88[lVar22] = (int)*(short *)(local_128 + lVar22 * 2) << 3;
                  lVar22 = lVar22 + 1;
                } while (lVar22 < lVar14);
                uVar9 = (ulonglong)local_130;
              }
              iVar23 = iVar16 + (int)uVar9;
              if (0 < iVar16) {
                if (0 < lVar14) {
                  lVar22 = local_108 + (longlong)(int)(iVar19 * local_138) * 4;
                  do {
                    piVar7 = (int *)(lVar13 * 4 + lVar22);
                    iVar4 = *(int *)((longlong)piVar7 + ((longlong)local_88 - lVar22));
                    iVar5 = local_c8[lVar13];
                    local_c8[lVar13] = iVar4;
                    lVar13 = lVar13 + 1;
                    *piVar7 = (iVar4 * (iVar2 - iVar16)) / iVar2 + iVar5;
                  } while (lVar13 < lVar14);
                  uVar9 = (ulonglong)local_130;
                }
                iVar19 = iVar19 + 1;
                iVar23 = iVar23 - iVar2;
                iVar24 = iVar24 + 1;
                if (local_134 <= iVar19) {
                  iVar19 = 0;
                }
              }
              if (iVar2 <= iVar23) {
                do {
                  lVar22 = 0;
                  puVar15 = (undefined4 *)(local_108 + (longlong)(int)(iVar19 * local_138) * 4);
                  if (0 < lVar14) {
                    lVar13 = (longlong)local_88 - (longlong)puVar15;
                    do {
                      lVar22 = lVar22 + 1;
                      *puVar15 = *(undefined4 *)((longlong)puVar15 + lVar13);
                      puVar15 = puVar15 + 1;
                    } while (lVar22 < lVar14);
                  }
                  iVar16 = iVar19 + 1;
                  iVar23 = iVar23 - iVar2;
                  iVar24 = iVar24 + 1;
                  iVar19 = 0;
                  if (iVar16 < local_134) {
                    iVar19 = iVar16;
                  }
                } while (iVar2 <= iVar23);
                uVar9 = (ulonglong)local_130;
              }
              lVar22 = 0;
              if (0 < lVar14) {
                do {
                  local_c8[lVar22] = (iVar23 * local_88[lVar22]) / iVar2;
                  lVar22 = lVar22 + 1;
                } while (lVar22 < lVar14);
              }
              local_128 = local_128 + local_118;
              uVar17 = (int)uVar20 - 1;
              uVar20 = (ulonglong)uVar17;
              local_120 = iVar19;
              iVar16 = iVar23;
            } while (0 < (int)uVar17);
          }
        }
      }
      else {
        if (param_6 != 0x18) {
          return 0xfffffff7;
        }
        iVar16 = param_5 * 3;
        uVar21 = (longlong)local_100 / (longlong)iVar16 & 0xffffffff;
        iVar24 = iVar4;
        if (0 < (int)((longlong)local_100 / (longlong)iVar16)) {
          if ((int)uVar17 < iVar2) {
            do {
              lVar22 = 0;
              uVar18 = uVar17;
              if (iVar2 - iVar23 < (int)uVar17) {
                uVar18 = iVar2 - iVar23;
              }
              if (0 < lVar14) {
                puVar11 = (undefined2 *)(local_128 + 1);
                do {
                  iVar5 = (int)((uint)CONCAT21(*puVar11,*(undefined1 *)((longlong)puVar11 + -1)) <<
                               8) >> 0xd;
                  iVar4 = (int)(uVar18 * iVar5) / (int)uVar17;
                  local_c8[lVar22] = local_c8[lVar22] + iVar4;
                  local_88[lVar22] = iVar5 - iVar4;
                  lVar22 = lVar22 + 1;
                  puVar11 = (undefined2 *)((longlong)puVar11 + 3);
                } while (lVar22 < lVar14);
              }
              lVar22 = 0;
              iVar23 = iVar23 + uVar18;
              if (iVar23 == iVar2) {
                if (0 < lVar14) {
                  do {
                    *(int *)((longlong)(local_88 + lVar22) +
                            ((local_108 + (longlong)(int)(iVar19 * local_138) * 4) -
                            (longlong)local_88)) = (int)(uVar17 * local_c8[lVar22]) / iVar2;
                    local_c8[lVar22] = local_88[lVar22];
                    lVar22 = lVar22 + 1;
                  } while (lVar22 < lVar14);
                }
                iVar19 = iVar19 + 1;
                iVar23 = uVar17 - uVar18;
                iVar24 = iVar24 + 1;
                if (local_134 <= iVar19) {
                  iVar19 = 0;
                }
              }
              uVar18 = (int)uVar21 - 1;
              uVar21 = (ulonglong)uVar18;
              local_128 = local_128 + iVar16;
              local_120 = iVar19;
            } while (0 < (int)uVar18);
          }
          else {
            local_118 = (longlong)iVar16;
            iVar16 = iVar23;
            do {
              lVar22 = 0;
              if (0 < lVar14) {
                puVar11 = (undefined2 *)(local_128 + 1);
                do {
                  local_88[lVar22] =
                       (int)((uint)CONCAT21(*puVar11,*(undefined1 *)((longlong)puVar11 + -1)) << 8)
                       >> 0xd;
                  lVar22 = lVar22 + 1;
                  puVar11 = (undefined2 *)((longlong)puVar11 + 3);
                } while (lVar22 < lVar14);
              }
              lVar22 = 0;
              iVar23 = iVar16 + (int)uVar9;
              if (0 < iVar16) {
                local_f8 = iVar2 - iVar16;
                lVar13 = local_108 + (longlong)(int)(iVar19 * local_138) * 4;
                if (0 < lVar14) {
                  do {
                    piVar7 = (int *)(lVar13 + lVar22 * 4);
                    iVar16 = *(int *)((longlong)piVar7 + ((longlong)local_88 - lVar13));
                    iVar4 = local_c8[lVar22];
                    local_c8[lVar22] = iVar16;
                    lVar22 = lVar22 + 1;
                    *piVar7 = (iVar16 * local_f8) / iVar2 + iVar4;
                  } while (lVar22 < lVar14);
                  uVar9 = (ulonglong)local_130;
                }
                iVar19 = iVar19 + 1;
                iVar23 = iVar23 - iVar2;
                iVar24 = iVar24 + 1;
                if (local_134 <= iVar19) {
                  iVar19 = 0;
                }
              }
              if (iVar2 <= iVar23) {
                do {
                  lVar22 = 0;
                  puVar15 = (undefined4 *)(local_108 + (longlong)(int)(iVar19 * local_138) * 4);
                  if (0 < lVar14) {
                    lVar13 = (longlong)local_88 - (longlong)puVar15;
                    do {
                      lVar22 = lVar22 + 1;
                      *puVar15 = *(undefined4 *)(lVar13 + (longlong)puVar15);
                      puVar15 = puVar15 + 1;
                    } while (lVar22 < lVar14);
                  }
                  iVar16 = iVar19 + 1;
                  iVar23 = iVar23 - iVar2;
                  iVar24 = iVar24 + 1;
                  iVar19 = 0;
                  if (iVar16 < local_134) {
                    iVar19 = iVar16;
                  }
                } while (iVar2 <= iVar23);
                uVar9 = (ulonglong)local_130;
              }
              lVar22 = 0;
              if (0 < lVar14) {
                do {
                  local_c8[lVar22] = (iVar23 * local_88[lVar22]) / iVar2;
                  lVar22 = lVar22 + 1;
                } while (lVar22 < lVar14);
              }
              uVar17 = (int)uVar21 - 1;
              uVar21 = (ulonglong)uVar17;
              local_128 = local_128 + local_118;
              local_120 = iVar19;
              iVar16 = iVar23;
            } while (0 < (int)uVar17);
          }
        }
      }
      lVar22 = 0;
      *(int *)(local_f0 + 0x34) = iVar23;
      if (0 < lVar14) {
        lVar13 = local_f0 + 0x38;
        do {
          piVar7 = local_c8 + lVar22;
          lVar12 = lVar22 * 4;
          lVar22 = lVar22 + 1;
          *(int *)(lVar13 + lVar12) = *piVar7;
          iVar19 = local_120;
        } while (lVar22 < lVar14);
      }
      goto LAB_1400038ea;
    }
  }
  lVar14 = (longlong)(int)local_e8;
  local_d8 = lVar14;
  if (0 < lVar14) {
    FUN_140007680((undefined8 *)local_c8,(undefined8 *)(param_1 + 0x13c),lVar14 << 2);
  }
  puVar15 = (undefined4 *)((int)local_118 + local_f0);
  local_100 = (iVar23 == 4) + 1;
  if (iVar23 == 8) {
    local_100 = 3;
  }
  else if (iVar23 == 0x10) {
    local_100 = 4;
  }
  sVar3 = (sbyte)local_100;
  if (param_6 == 8) {
    if (0 < iVar2) {
      lVar13 = local_128;
      iVar24 = iVar2;
      do {
        lVar12 = 0;
        iVar19 = iVar23 + -1;
        iVar16 = 0;
        if (0 < lVar14) {
          do {
            local_88[lVar12] = (*(byte *)(lVar12 + lVar13) - 0x80) * 0x800;
            lVar12 = lVar12 + 1;
          } while (lVar12 < lVar14);
        }
        if (0 < iVar19) {
          do {
            lVar12 = 0;
            if (0 < lVar14) {
              do {
                puVar15[lVar12] =
                     iVar19 * local_c8[lVar12] + (iVar16 + 1) * local_88[lVar12] >> sVar3;
                lVar12 = lVar12 + 1;
              } while (lVar12 < lVar14);
            }
            iVar16 = iVar16 + 1;
            puVar15 = puVar15 + (int)local_138;
            iVar19 = iVar19 + -1;
          } while (iVar16 < iVar23 + -1);
        }
        lVar12 = 0;
        if (0 < lVar14) {
          puVar8 = puVar15;
          do {
            uVar1 = *(undefined4 *)((longlong)puVar8 + ((longlong)local_88 - (longlong)puVar15));
            lVar12 = lVar12 + 1;
            *puVar8 = uVar1;
            *(undefined4 *)((longlong)puVar8 + ((longlong)local_c8 - (longlong)puVar15)) = uVar1;
            puVar8 = puVar8 + 1;
          } while (lVar12 < lVar14);
        }
        puVar15 = puVar15 + (int)local_138;
        lVar13 = lVar13 + lVar22;
        iVar24 = iVar24 + -1;
        iVar19 = local_120;
        local_118 = lVar22;
      } while (0 < iVar24);
    }
  }
  else if (param_6 == 0x10) {
    if (0 < iVar2) {
      local_118 = lVar22 * 2;
      lVar22 = local_128;
      iVar24 = iVar2;
      do {
        lVar13 = 0;
        iVar19 = iVar23 + -1;
        iVar16 = 0;
        if (0 < lVar14) {
          do {
            local_88[lVar13] = (int)*(short *)(lVar22 + lVar13 * 2) << 3;
            lVar13 = lVar13 + 1;
          } while (lVar13 < lVar14);
        }
        if (0 < iVar19) {
          do {
            lVar13 = 0;
            if (0 < lVar14) {
              do {
                puVar15[lVar13] =
                     iVar19 * local_c8[lVar13] + (iVar16 + 1) * local_88[lVar13] >> sVar3;
                lVar13 = lVar13 + 1;
              } while (lVar13 < lVar14);
            }
            iVar16 = iVar16 + 1;
            puVar15 = puVar15 + (int)local_138;
            iVar19 = iVar19 + -1;
          } while (iVar16 < iVar23 + -1);
        }
        lVar13 = 0;
        if (0 < lVar14) {
          puVar8 = puVar15;
          do {
            uVar1 = *(undefined4 *)((longlong)puVar8 + ((longlong)local_88 - (longlong)puVar15));
            lVar13 = lVar13 + 1;
            *puVar8 = uVar1;
            *(undefined4 *)((longlong)puVar8 + ((longlong)local_c8 - (longlong)puVar15)) = uVar1;
            puVar8 = puVar8 + 1;
          } while (lVar13 < lVar14);
        }
        puVar15 = puVar15 + (int)local_138;
        lVar22 = lVar22 + local_118;
        iVar24 = iVar24 + -1;
        iVar19 = local_120;
      } while (0 < iVar24);
    }
  }
  else {
    if (param_6 != 0x18) {
      return 0xffffffce;
    }
    if (0 < iVar2) {
      iVar24 = iVar2;
      do {
        iVar19 = iVar23 + -1;
        lVar22 = 0;
        if (0 < lVar14) {
          puVar11 = (undefined2 *)(local_128 + 1);
          do {
            local_88[lVar22] =
                 (int)((uint)CONCAT21(*puVar11,*(undefined1 *)((longlong)puVar11 + -1)) << 8) >> 0xd
            ;
            lVar22 = lVar22 + 1;
            puVar11 = (undefined2 *)((longlong)puVar11 + 3);
          } while (lVar22 < lVar14);
        }
        iVar16 = 0;
        if (0 < iVar19) {
          do {
            lVar22 = 0;
            if (0 < lVar14) {
              do {
                puVar15[lVar22] =
                     (iVar16 + 1) * local_88[lVar22] + iVar19 * local_c8[lVar22] >> sVar3;
                lVar22 = lVar22 + 1;
              } while (lVar22 < lVar14);
            }
            iVar16 = iVar16 + 1;
            puVar15 = puVar15 + (int)local_138;
            iVar19 = iVar19 + -1;
          } while (iVar16 < iVar23 + -1);
        }
        lVar22 = 0;
        if (0 < lVar14) {
          puVar8 = puVar15;
          do {
            uVar1 = *(undefined4 *)((longlong)puVar8 + ((longlong)local_88 - (longlong)puVar15));
            lVar22 = lVar22 + 1;
            *puVar8 = uVar1;
            *(undefined4 *)((longlong)puVar8 + ((longlong)local_c8 - (longlong)puVar15)) = uVar1;
            puVar8 = puVar8 + 1;
          } while (lVar22 < lVar14);
        }
        puVar15 = puVar15 + (int)local_138;
        local_128 = local_128 + (int)(param_5 * 3);
        iVar24 = iVar24 + -1;
        iVar19 = local_120;
      } while (0 < iVar24);
    }
  }
  lVar22 = 0;
  if (0 < lVar14) {
    do {
      *(int *)(local_f0 + 0x13c + lVar22 * 4) = local_c8[lVar22];
      lVar22 = lVar22 + 1;
    } while (lVar22 < lVar14);
  }
  lVar22 = 0;
  puVar25 = (undefined8 *)(*(int *)(local_f0 + 0x24) + local_f0);
  iVar24 = iVar23 * iVar2;
  local_fc = iVar2;
  if (param_4 * iVar23 == local_e4) {
    if (0 < iVar24) {
      uVar9 = (ulonglong)(int)local_138;
      lVar22 = uVar9 * 4;
      iVar23 = iVar24;
      do {
        lVar13 = 0;
        puVar10 = (undefined8 *)(local_108 + (longlong)(iVar19 * (int)uVar9) * 4);
        if ((((int)local_e8 < 1) || (local_e8 < 4)) ||
           ((puVar10 <= (undefined8 *)((longlong)puVar25 + (longlong)(int)(local_e8 - 1) * 4) &&
            (puVar25 <= (undefined8 *)((longlong)puVar10 + (longlong)(int)(local_e8 - 1) * 4))))) {
          if (0 < lVar14) {
            lVar12 = (longlong)puVar25 - (longlong)puVar10;
            do {
              lVar13 = lVar13 + 1;
              *(undefined4 *)puVar10 = *(undefined4 *)(lVar12 + (longlong)puVar10);
              puVar10 = (undefined8 *)((longlong)puVar10 + 4);
            } while (lVar13 < lVar14);
          }
        }
        else {
          FUN_140007680(puVar10,puVar25,lVar14 * 4);
          do {
            lVar13 = lVar13 + 1;
          } while (lVar13 < lVar14);
        }
        iVar2 = iVar19 + 1;
        uVar9 = (ulonglong)local_138;
        iVar19 = 0;
        if (iVar2 < local_134) {
          iVar19 = iVar2;
        }
        puVar25 = (undefined8 *)((longlong)puVar25 + lVar22);
        iVar23 = iVar23 + -1;
      } while (0 < iVar23);
    }
  }
  else {
    uVar17 = *(uint *)(local_f0 + 0x34);
    iVar2 = 0;
    uVar18 = (param_4 * iVar23) / local_f8;
    if (0 < lVar14) {
      do {
        local_c8[lVar22] = *(int *)(local_f0 + lVar22 * 4 + 0x38);
        lVar22 = lVar22 + 1;
      } while (lVar22 < lVar14);
    }
    if (0 < iVar24) {
      if ((int)local_130 < (int)uVar18) {
        uVar9 = (ulonglong)(int)local_138;
        local_128 = uVar9 * 4;
        do {
          lVar22 = 0;
          uVar6 = local_130;
          if ((int)(uVar18 - uVar17) < (int)local_130) {
            uVar6 = uVar18 - uVar17;
          }
          if (0 < lVar14) {
            do {
              piVar7 = local_c8 + lVar22;
              iVar23 = *(int *)((longlong)piVar7 + ((longlong)puVar25 - (longlong)local_c8));
              iVar16 = (int)(iVar23 * uVar6) / (int)local_130;
              *piVar7 = *piVar7 + iVar16;
              local_88[lVar22] = iVar23 - iVar16;
              lVar22 = lVar22 + 1;
            } while (lVar22 < lVar14);
            uVar9 = (ulonglong)local_138;
          }
          uVar17 = uVar17 + uVar6;
          if (uVar17 == uVar18) {
            lVar22 = 0;
            if (0 < lVar14) {
              do {
                *(int *)((longlong)(local_88 + lVar22) +
                        ((local_108 + (longlong)(iVar19 * (int)uVar9) * 4) - (longlong)local_88)) =
                     (int)(local_130 * local_c8[lVar22]) / (int)uVar18;
                local_c8[lVar22] = local_88[lVar22];
                lVar22 = lVar22 + 1;
              } while (lVar22 < lVar14);
              uVar9 = (ulonglong)local_138;
            }
            iVar19 = iVar19 + 1;
            uVar17 = local_130 - uVar6;
            iVar2 = iVar2 + 1;
            if (local_134 <= iVar19) {
              iVar19 = 0;
            }
          }
          puVar25 = (undefined8 *)((longlong)puVar25 + local_128);
          iVar24 = iVar24 + -1;
        } while (0 < iVar24);
      }
      else {
        uVar9 = (ulonglong)(int)local_138;
        local_128 = uVar9 * 4;
        iVar23 = iVar19;
        do {
          while ((int)uVar18 <= (int)uVar17) {
            lVar22 = 0;
            puVar15 = (undefined4 *)(local_108 + (longlong)(iVar23 * (int)uVar9) * 4);
            if (0 < lVar14) {
              lVar13 = (longlong)local_c8 - (longlong)puVar15;
              do {
                lVar22 = lVar22 + 1;
                *puVar15 = *(undefined4 *)((longlong)puVar15 + lVar13);
                puVar15 = puVar15 + 1;
              } while (lVar22 < lVar14);
              uVar9 = (ulonglong)local_138;
            }
            iVar19 = iVar23 + 1;
            uVar17 = uVar17 - uVar18;
            iVar2 = iVar2 + 1;
            iVar23 = 0;
            if (iVar19 < local_134) {
              iVar23 = iVar19;
            }
          }
          lVar13 = 0;
          lVar22 = lVar13;
          if (0 < lVar14) {
            do {
              local_88[lVar22] = *(int *)((longlong)puVar25 + lVar22 * 4);
              lVar22 = lVar22 + 1;
            } while (lVar22 < lVar14);
          }
          iVar19 = iVar23;
          uVar6 = local_130;
          if (0 < (int)uVar17) {
            if (0 < lVar14) {
              do {
                *(int *)(local_108 + (longlong)(iVar23 * (int)uVar9) * 4 + lVar13 * 4) =
                     (int)(uVar17 * local_c8[lVar13] + (uVar18 - uVar17) * local_88[lVar13]) /
                     (int)uVar18;
                lVar13 = lVar13 + 1;
              } while (lVar13 < lVar14);
            }
            uVar9 = (ulonglong)local_138;
            iVar2 = iVar2 + 1;
            uVar6 = local_130 - (uVar18 - uVar17);
            iVar19 = iVar23 + 1;
            if (local_134 <= iVar23 + 1) {
              iVar19 = 0;
            }
          }
          while (uVar17 = uVar6, (int)uVar18 <= (int)uVar17) {
            lVar22 = 0;
            puVar15 = (undefined4 *)(local_108 + (longlong)(iVar19 * (int)uVar9) * 4);
            if (0 < lVar14) {
              lVar13 = (longlong)local_88 - (longlong)puVar15;
              do {
                lVar22 = lVar22 + 1;
                *puVar15 = *(undefined4 *)((longlong)puVar15 + lVar13);
                puVar15 = puVar15 + 1;
              } while (lVar22 < lVar14);
              uVar9 = (ulonglong)local_138;
            }
            iVar23 = iVar19 + 1;
            iVar2 = iVar2 + 1;
            iVar19 = 0;
            uVar6 = uVar17 - uVar18;
            if (iVar23 < local_134) {
              iVar19 = iVar23;
            }
          }
          lVar22 = 0;
          if (0 < lVar14) {
            do {
              local_c8[lVar22] = local_88[lVar22];
              lVar22 = lVar22 + 1;
            } while (lVar22 < lVar14);
          }
          puVar25 = (undefined8 *)((longlong)puVar25 + local_128);
          iVar24 = iVar24 + -1;
          iVar23 = iVar19;
        } while (0 < iVar24);
      }
    }
    iVar24 = iVar2;
    lVar22 = 0;
    *(uint *)(local_f0 + 0x34) = uVar17;
    if (0 < lVar14) {
      lVar13 = local_f0 + 0x38;
      do {
        piVar7 = local_c8 + lVar22;
        lVar12 = lVar22 * 4;
        lVar22 = lVar22 + 1;
        *(int *)(lVar13 + lVar12) = *piVar7;
      } while (lVar22 < lVar14);
    }
  }
LAB_1400038ea:
  lVar22 = local_d8;
  iVar23 = local_134;
  if (((param_7 != 0) && ((int)local_e8 < (int)local_138)) && (iVar19 = *local_e0, 0 < iVar24)) {
    iVar2 = local_138 - local_e8;
    do {
      FUN_140007940((longlong *)(local_108 + ((int)(iVar19 * local_138) + lVar22) * 4),0,
                    (undefined1 *)((longlong)iVar2 << 2));
      iVar16 = iVar19 + 1;
      iVar19 = 0;
      if (iVar16 < iVar23) {
        iVar19 = iVar16;
      }
      iVar24 = iVar24 + -1;
    } while (0 < iVar24);
  }
  *local_e0 = iVar19;
  return 0;
}



// === FUN_140003968 @ 140003968 (size=66) ===

void FUN_140003968(longlong param_1)

{
  undefined4 *puVar1;
  longlong lVar2;
  
  puVar1 = (undefined4 *)(param_1 + 0xbc);
  lVar2 = 0x10;
  do {
    puVar1[-0x21] = 0;
    *puVar1 = 0;
    puVar1[-0x11] = 0;
    puVar1[0x10] = 0;
    puVar1[0x20] = 0;
    puVar1 = puVar1 + 1;
    lVar2 = lVar2 + -1;
  } while (lVar2 != 0);
  *(undefined8 *)(param_1 + 0x18) = 0;
  *(undefined4 *)(param_1 + 0x34) = 0;
  *(undefined4 *)(param_1 + 0xb8) = 0;
  return;
}



// === FUN_1400039ac @ 1400039ac (size=85) ===

undefined8 FUN_1400039ac(longlong param_1)

{
  int iVar1;
  
  iVar1 = *(int *)(param_1 + 0x10) * *(int *)(param_1 + 0xc);
  if (0 < iVar1) {
    FUN_140007940((longlong *)(*(int *)(param_1 + 8) + param_1),0,
                  (undefined1 *)((longlong)iVar1 << 2));
  }
  FUN_140003968(param_1);
  *(undefined4 *)(param_1 + 0x17c) = 0;
  *(undefined4 *)(param_1 + 0x180) = 0;
  *(undefined4 *)(param_1 + 0x184) = 0;
  *(undefined4 *)(param_1 + 0x188) = 0;
  return 0;
}



// === FUN_140003a04 @ 140003a04 (size=79) ===

longlong * FUN_140003a04(undefined8 param_1,undefined1 *param_2)

{
  longlong *plVar1;
  
  if (DAT_140012c80 == 0) {
    plVar1 = (longlong *)ExAllocatePoolWithTag(0);
    if (plVar1 != (longlong *)0x0) {
      FUN_140007940(plVar1,0,param_2);
    }
  }
  else {
    plVar1 = (longlong *)(*(code *)PTR__guard_dispatch_icall_140008188)();
  }
  return plVar1;
}



// === FUN_140003a54 @ 140003a54 (size=20) ===

void FUN_140003a54(longlong param_1)

{
  if (param_1 != 0) {
    ExFreePoolWithTag();
  }
  return;
}



// === FUN_140003a68 @ 140003a68 (size=45) ===

void FUN_140003a68(void)

{
  undefined1 local_18 [24];
  
  RtlInitUnicodeString(local_18,L"ExAllocatePool2");
  DAT_140012c80 = MmGetSystemRoutineAddress(local_18);
  return;
}



// === FUN_140003aa0 @ 140003aa0 (size=1322) ===

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

ulonglong FUN_140003aa0(undefined8 param_1,longlong param_2)

{
  ulonglong *puVar1;
  int iVar2;
  ulonglong *puVar3;
  ulonglong uVar4;
  uint *puVar5;
  undefined *puVar6;
  longlong lVar7;
  uint uVar8;
  uint uVar9;
  ulonglong *puVar10;
  ulonglong *puVar11;
  bool bVar12;
  uint uVar13;
  uint uVar14;
  uint uVar15;
  
  uVar4 = _DAT_140012ca8;
  lVar7 = *(longlong *)(param_2 + 0xb8);
  uVar15 = 0;
  _DAT_14000eae4 = _DAT_14000eae4 + 1;
  uVar9 = 0xc00000bb;
  uVar8 = *(uint *)(lVar7 + 0x18);
  uVar13 = *(uint *)(lVar7 + 0x10);
  uVar14 = *(uint *)(lVar7 + 8);
  puVar11 = *(ulonglong **)(param_2 + 0x18);
  if (uVar8 < 0x222045) {
    if (uVar8 == 0x222044) {
      if (((puVar11 != (ulonglong *)0x0) && (3 < uVar13)) && (0x1f < uVar14)) {
        *puVar11 = _DAT_140012ca0;
        puVar11[1] = uVar4;
        uVar8 = _DAT_140012cb0;
        uVar13 = uRam0000000140012cb4;
        uVar14 = uRam0000000140012cb8;
        uVar15 = uRam0000000140012cbc;
LAB_140003ccf:
        *(uint *)(puVar11 + 2) = uVar8;
        *(uint *)((longlong)puVar11 + 0x14) = uVar13;
        *(uint *)(puVar11 + 3) = uVar14;
        *(uint *)((longlong)puVar11 + 0x1c) = uVar15;
        uVar15 = 0x20;
LAB_140003f99:
        uVar9 = 0;
        goto LAB_140003f9b;
      }
    }
    else if (uVar8 == 0x222010) {
      if (((puVar11 != (ulonglong *)0x0) && (3 < uVar13)) && (3 < uVar14)) {
        DAT_14000eae0 = DAT_14000eae0 * _DAT_14000eae4;
        _DAT_14000eae4 = DAT_14000eae0 * DAT_14000eae0 + 1;
        *(uint *)puVar11 = 0xffffffff;
        goto LAB_140003f97;
      }
    }
    else if ((uVar8 == 0x222014) || (uVar8 == 0x222018)) {
      if ((puVar11 != (ulonglong *)0x0) && (0x1f < uVar13)) {
        uVar8 = (uint)puVar11[2];
joined_r0x000140003c61:
        if (uVar8 != DAT_14000eae0) goto LAB_140003f9b;
      }
    }
    else if (uVar8 == 0x22201c) {
      if ((puVar11 != (ulonglong *)0x0) && (0x1f < uVar13)) {
        uVar8 = (uint)puVar11[2];
        goto joined_r0x000140003c61;
      }
    }
    else if (uVar8 == 0x222024) {
      if ((puVar11 != (ulonglong *)0x0) && (0x6f < uVar14)) {
        puVar3 = (ulonglong *)FUN_140004080(0);
        if (puVar3 == (ulonglong *)0x0) goto LAB_140003f9b;
        if (uVar14 < 0x1a0) goto LAB_140003be3;
        lVar7 = 3;
        do {
          uVar4 = puVar3[1];
          *puVar11 = *puVar3;
          puVar11[1] = uVar4;
          uVar4 = puVar3[3];
          puVar11[2] = puVar3[2];
          puVar11[3] = uVar4;
          uVar4 = puVar3[5];
          puVar11[4] = puVar3[4];
          puVar11[5] = uVar4;
          uVar4 = puVar3[7];
          puVar11[6] = puVar3[6];
          puVar11[7] = uVar4;
          uVar4 = puVar3[9];
          puVar11[8] = puVar3[8];
          puVar11[9] = uVar4;
          uVar4 = puVar3[0xb];
          puVar11[10] = puVar3[10];
          puVar11[0xb] = uVar4;
          uVar4 = puVar3[0xd];
          puVar11[0xc] = puVar3[0xc];
          puVar11[0xd] = uVar4;
          puVar10 = puVar11 + 0x10;
          puVar1 = puVar3 + 0xe;
          uVar4 = puVar3[0xf];
          puVar3 = puVar3 + 0x10;
          puVar11[0xe] = *puVar1;
          puVar11[0xf] = uVar4;
          lVar7 = lVar7 + -1;
          puVar11 = puVar10;
        } while (lVar7 != 0);
LAB_140003bd0:
        uVar15 = 0x1a0;
        uVar4 = puVar3[1];
        *puVar10 = *puVar3;
        puVar10[1] = uVar4;
        uVar4 = puVar3[3];
        puVar10[2] = puVar3[2];
        puVar10[3] = uVar4;
        goto LAB_140003f99;
      }
    }
    else {
      if (uVar8 != 0x222040) {
LAB_140003d12:
        uVar4 = PcDispatchIrp(param_1,param_2);
        return uVar4;
      }
      if ((puVar11 != (ulonglong *)0x0) && (3 < uVar14)) {
        *(uint *)puVar11 = 0x3030109;
        goto LAB_140003f97;
      }
    }
  }
  else if (uVar8 == 0x222048) {
    if (((puVar11 != (ulonglong *)0x0) && (3 < uVar13)) && (3 < uVar14)) {
      uVar8 = (uint)*puVar11;
      if ((((uVar8 != 32000) && (uVar8 != 0xac44)) &&
          ((uVar8 != 48000 && ((uVar8 != 0x15888 && (uVar8 != 0x2b110)))))) && (uVar8 != 0x2ee00)) {
        uVar8 = 96000;
      }
      lVar7 = FUN_140004068(0);
      if (lVar7 != 0) {
        *(uint *)(lVar7 + 4) = uVar8;
      }
      lVar7 = FUN_140004050(0);
      if (lVar7 != 0) {
        *(uint *)(lVar7 + 4) = uVar8;
      }
      puVar6 = FUN_140004080(0);
      if (puVar6 != (undefined *)0x0) {
        *(uint *)(puVar6 + 4) = uVar8;
      }
LAB_140003f95:
      *(uint *)puVar11 = uVar8;
LAB_140003f97:
      uVar15 = 4;
      goto LAB_140003f99;
    }
  }
  else if (uVar8 == 0x22204c) {
    if (((puVar11 != (ulonglong *)0x0) && (3 < uVar13)) && (3 < uVar14)) {
      uVar8 = (uint)*puVar11;
      if ((uint)*puVar11 < 0x30) {
        uVar8 = 0x30;
      }
      puVar5 = (uint *)FUN_140004068(0);
      if (puVar5 != (uint *)0x0) {
        *puVar5 = uVar8;
      }
      puVar5 = (uint *)FUN_140004050(0);
      if (puVar5 != (uint *)0x0) {
        *puVar5 = uVar8;
      }
      iVar2 = 0;
LAB_140003d85:
      puVar6 = FUN_140004080(iVar2);
      if (puVar6 != (undefined *)0x0) {
        *(uint *)(puVar6 + 0x19c) = uVar8;
      }
      goto LAB_140003f95;
    }
  }
  else if (uVar8 == 0x222050) {
    if (((puVar11 != (ulonglong *)0x0) && (0xf < uVar13)) &&
       ((iVar2 = (int)*puVar11, -1 < iVar2 && ((iVar2 < 1 && (0x6f < uVar14)))))) {
      puVar3 = (ulonglong *)FUN_140004080(iVar2);
      if (puVar3 == (ulonglong *)0x0) goto LAB_140003f9b;
      if (0x19f < uVar14) {
        lVar7 = 3;
        do {
          uVar4 = puVar3[1];
          *puVar11 = *puVar3;
          puVar11[1] = uVar4;
          uVar4 = puVar3[3];
          puVar11[2] = puVar3[2];
          puVar11[3] = uVar4;
          uVar4 = puVar3[5];
          puVar11[4] = puVar3[4];
          puVar11[5] = uVar4;
          uVar4 = puVar3[7];
          puVar11[6] = puVar3[6];
          puVar11[7] = uVar4;
          uVar4 = puVar3[9];
          puVar11[8] = puVar3[8];
          puVar11[9] = uVar4;
          uVar4 = puVar3[0xb];
          puVar11[10] = puVar3[10];
          puVar11[0xb] = uVar4;
          uVar4 = puVar3[0xd];
          puVar11[0xc] = puVar3[0xc];
          puVar11[0xd] = uVar4;
          puVar10 = puVar11 + 0x10;
          puVar1 = puVar3 + 0xe;
          uVar4 = puVar3[0xf];
          puVar3 = puVar3 + 0x10;
          puVar11[0xe] = *puVar1;
          puVar11[0xf] = uVar4;
          lVar7 = lVar7 + -1;
          puVar11 = puVar10;
        } while (lVar7 != 0);
        goto LAB_140003bd0;
      }
LAB_140003be3:
      uVar4 = puVar3[1];
      uVar15 = 0x70;
      *puVar11 = *puVar3;
      puVar11[1] = uVar4;
      uVar4 = puVar3[3];
      puVar11[2] = puVar3[2];
      puVar11[3] = uVar4;
      uVar4 = puVar3[5];
      puVar11[4] = puVar3[4];
      puVar11[5] = uVar4;
      uVar4 = puVar3[7];
      puVar11[6] = puVar3[6];
      puVar11[7] = uVar4;
      uVar4 = puVar3[9];
      puVar11[8] = puVar3[8];
      puVar11[9] = uVar4;
      uVar4 = puVar3[0xb];
      puVar11[10] = puVar3[10];
      puVar11[0xb] = uVar4;
      uVar4 = puVar3[0xd];
      puVar11[0xc] = puVar3[0xc];
      puVar11[0xd] = uVar4;
      goto LAB_140003f99;
    }
  }
  else if (uVar8 == 0x222054) {
    if ((((puVar11 != (ulonglong *)0x0) && (0xf < uVar13)) && (iVar2 = (int)*puVar11, -1 < iVar2))
       && ((iVar2 < 1 && (0x1f < uVar14)))) {
      lVar7 = (longlong)iVar2 * 0x20;
      uVar4 = *(ulonglong *)(&DAT_140012ca8 + lVar7);
      *puVar11 = *(ulonglong *)(&DAT_140012ca0 + lVar7);
      puVar11[1] = uVar4;
      uVar8 = *(uint *)(&DAT_140012cb0 + lVar7);
      uVar13 = *(uint *)(lVar7 + 0x140012cb4);
      uVar14 = *(uint *)(lVar7 + 0x140012cb8);
      uVar15 = *(uint *)(lVar7 + 0x140012cbc);
      goto LAB_140003ccf;
    }
  }
  else if (uVar8 == 0x222058) {
    if (((puVar11 != (ulonglong *)0x0) && (3 < uVar13)) && (3 < uVar14)) {
      bVar12 = (uint)*puVar11 == 1;
      FUN_140005370(0,(uint)bVar12);
      *(uint *)puVar11 = (uint)bVar12;
      goto LAB_140003f97;
    }
  }
  else {
    if (uVar8 != 0x22205c) goto LAB_140003d12;
    if ((puVar11 != (ulonglong *)0x0) && (0xf < uVar13)) {
      uVar4 = *puVar11;
      iVar2 = (int)uVar4;
      if ((-1 < iVar2) && ((iVar2 < 1 && (3 < uVar14)))) {
        uVar8 = (uint)(uVar4 >> 0x20);
        if (uVar8 < 0x30) {
          uVar8 = 0x30;
        }
        puVar5 = (uint *)FUN_140004068(uVar4 & 0xffffffff);
        if (puVar5 != (uint *)0x0) {
          *puVar5 = uVar8;
        }
        puVar5 = (uint *)FUN_140004050(uVar4 & 0xffffffff);
        if (puVar5 != (uint *)0x0) {
          *puVar5 = uVar8;
        }
        goto LAB_140003d85;
      }
    }
  }
  uVar9 = 0xc0000010;
  uVar15 = 0;
LAB_140003f9b:
  *(ulonglong *)(param_2 + 0x38) = (ulonglong)uVar15;
  *(uint *)(param_2 + 0x30) = uVar9;
  IofCompleteRequest(param_2,0);
  return (ulonglong)uVar9;
}



// === FUN_140003fd0 @ 140003fd0 (size=9) ===

void FUN_140003fd0(longlong param_1,uint param_2)

{
  FUN_140003fdc((undefined8 *)(param_1 + -0x10),param_2);
  return;
}



// === FUN_140003fdc @ 140003fdc (size=52) ===

undefined8 * FUN_140003fdc(undefined8 *param_1,uint param_2)

{
  FUN_14001566c(param_1);
  if ((param_2 & 1) != 0) {
    FUN_1400048b8((longlong)param_1);
  }
  return param_1;
}



// === FUN_140004010 @ 140004010 (size=18) ===

/* WARNING: Switch with 1 destination removed at 0x00014000401b */

void FUN_140004010(longlong param_1)

{
                    /* WARNING: Could not recover jumptable at 0x000140007650. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (**(code **)(**(longlong **)(param_1 + 0x20) + 8))();
  return;
}



// === FUN_140004030 @ 140004030 (size=9) ===

void FUN_140004030(longlong param_1)

{
  FUN_140004010(param_1 + -8);
  return;
}



// === FUN_14000403c @ 14000403c (size=20) ===

undefined * FUN_14000403c(ulonglong param_1)

{
  return &DAT_140012eb0 + (param_1 & 0xffffffff) * 0x2b0;
}



// === FUN_140004050 @ 140004050 (size=21) ===

undefined8 FUN_140004050(ulonglong param_1)

{
  return (&DAT_140012e68)[(param_1 & 0xffffffff) * 0x56];
}



// === FUN_140004068 @ 140004068 (size=21) ===

undefined8 FUN_140004068(ulonglong param_1)

{
  return (&DAT_140012e60)[(param_1 & 0xffffffff) * 0x56];
}



// === FUN_140004080 @ 140004080 (size=28) ===

undefined * FUN_140004080(int param_1)

{
  if (param_1 != 0) {
    return (undefined *)0x0;
  }
  return &DAT_140012cc0;
}



// === FUN_14000409c @ 14000409c (size=20) ===

undefined * FUN_14000409c(ulonglong param_1)

{
  return &DAT_140012e70 + (param_1 & 0xffffffff) * 0x2b0;
}



// === FUN_1400040b0 @ 1400040b0 (size=193) ===

void FUN_1400040b0(ulonglong param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4,
                  int param_5,undefined4 param_6)

{
  undefined4 *puVar1;
  longlong lVar2;
  int *piVar3;
  longlong lVar4;
  
  lVar2 = (param_1 & 0xffffffff) * 0x2b0;
  piVar3 = (int *)(&DAT_140012cc0 + lVar2);
  *piVar3 = *piVar3 + 1;
  puVar1 = (undefined4 *)(&DAT_140012d90 + lVar2);
  *(undefined4 *)(&DAT_140012cc8 + lVar2) = param_3;
  *(undefined4 *)(&DAT_140012cc4 + lVar2) = param_2;
  *(undefined8 *)(&DAT_140012ccc + lVar2) = 0;
  *(undefined8 *)((longlong)&DAT_140012cd0 + lVar2 + 4) = 0;
  lVar4 = 0x20;
  *(undefined8 *)(&DAT_140012cdc + lVar2) = 0;
  *(undefined8 *)(&DAT_140012ce4 + lVar2) = 0;
  *(undefined8 *)(&DAT_140012cec + lVar2) = 0;
  *(undefined8 *)(&DAT_140012cf4 + lVar2) = 0;
  *(undefined8 *)(&DAT_140012cfc + lVar2) = 0;
  *(undefined8 *)(&DAT_140012d04 + lVar2) = 0;
  *(undefined4 *)(&DAT_140012d0c + lVar2) = 0;
  do {
    puVar1[-0x20] = 0;
    *puVar1 = 0;
    puVar1 = puVar1 + 1;
    lVar4 = lVar4 + -1;
  } while (lVar4 != 0);
  *(undefined4 *)(&DAT_140012e34 + lVar2) = param_6;
  *(undefined8 *)(&DAT_140012e10 + lVar2) = 0;
  *(undefined8 *)(&DAT_140012e18 + lVar2) = 0;
  *(undefined8 *)(&DAT_140012e20 + lVar2) = 0;
  *(undefined8 *)(&DAT_140012e28 + lVar2) = 0;
  *(undefined4 *)(&DAT_140012e30 + lVar2) = 0;
  *(undefined8 *)(&DAT_140012e38 + lVar2) = 0;
  *(undefined8 *)(&DAT_140012e40 + lVar2) = 0;
  *(undefined8 *)(&DAT_140012e48 + lVar2) = 0;
  *(undefined8 *)(&DAT_140012e50 + lVar2) = 0;
  *(undefined4 *)(&DAT_140012e58 + lVar2) = 0;
  *(undefined4 *)(&DAT_140012e5c + lVar2) = param_4;
  if (param_5 != 0) {
    *piVar3 = param_5;
  }
  return;
}



// === FUN_140004180 @ 140004180 (size=30) ===

undefined4 FUN_140004180(longlong param_1,uint param_2)

{
  if (*(longlong *)(param_1 + 0x50) == 0) {
    return 0;
  }
  if (param_2 < 0x14) {
    return *(undefined4 *)(*(longlong *)(param_1 + 0x50) + (ulonglong)param_2 * 4);
  }
  return 0;
}



// === FUN_140004190 @ 140004190 (size=23) ===

void FUN_140004190(longlong param_1,uint param_2,undefined8 param_3,undefined4 param_4)

{
  if (*(longlong *)(param_1 + 0x50) != 0) {
    FUN_140004544(*(longlong *)(param_1 + 0x50),param_2,param_3,param_4);
  }
  return;
}



// === FUN_1400041b0 @ 1400041b0 (size=23) ===

undefined4 FUN_1400041b0(longlong param_1)

{
  if (*(longlong *)(param_1 + 0x50) == 0) {
    return 0;
  }
  return *(undefined4 *)(*(longlong *)(param_1 + 0x50) + 0xf0);
}



// === FUN_1400041c0 @ 1400041c0 (size=23) ===

void FUN_1400041c0(longlong param_1,undefined4 param_2)

{
  if (*(longlong *)(param_1 + 0x50) != 0) {
    FUN_140004550(*(longlong *)(param_1 + 0x50),param_2);
  }
  return;
}



// === FUN_1400041e0 @ 1400041e0 (size=34) ===

undefined4 FUN_1400041e0(longlong param_1,uint param_2)

{
  if (*(longlong *)(param_1 + 0x50) == 0) {
    return 0;
  }
  if (param_2 < 0x14) {
    return *(undefined4 *)(*(longlong *)(param_1 + 0x50) + 0xa0 + (ulonglong)param_2 * 4);
  }
  return 0;
}



// === FUN_1400041f0 @ 1400041f0 (size=31) ===

undefined4 FUN_1400041f0(longlong param_1,uint param_2)

{
  if (*(longlong *)(param_1 + 0x50) == 0) {
    return 0;
  }
  if (param_2 < 0x14) {
    return *(undefined4 *)(*(longlong *)(param_1 + 0x50) + 0x50 + (ulonglong)param_2 * 4);
  }
  return 0;
}



// === FUN_140004200 @ 140004200 (size=23) ===

void FUN_140004200(longlong param_1,uint param_2,undefined8 param_3,undefined4 param_4)

{
  if (*(longlong *)(param_1 + 0x50) != 0) {
    FUN_140004558(*(longlong *)(param_1 + 0x50),param_2,param_3,param_4);
  }
  return;
}



// === FUN_140004220 @ 140004220 (size=122) ===

void FUN_140004220(longlong param_1,int param_2)

{
  undefined8 *puVar1;
  
  for (puVar1 = *(undefined8 **)(param_1 + 0x60); puVar1 != (undefined8 *)(param_1 + 0x60);
      puVar1 = (undefined8 *)*puVar1) {
    if (puVar1[0x45] != 0) {
      (*(code *)PTR__guard_dispatch_icall_140008188)(puVar1[0x45],param_2);
    }
  }
  if ((param_2 != *(int *)(param_1 + 0x40)) &&
     ((((param_2 == 1 || (param_2 == 2)) || (param_2 == 3)) || (param_2 == 4)))) {
    *(int *)(param_1 + 0x40) = param_2;
  }
  return;
}



// === FUN_1400042a0 @ 1400042a0 (size=3) ===

undefined8 FUN_1400042a0(void)

{
  return 0;
}



// === FUN_1400042b0 @ 1400042b0 (size=17) ===

/* WARNING: Switch with 1 destination removed at 0x0001400042ba */

void FUN_1400042b0(longlong param_1)

{
                    /* WARNING: Could not recover jumptable at 0x000140007650. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (**(code **)**(undefined8 **)(param_1 + 0x20))();
  return;
}



// === FUN_1400042d0 @ 1400042d0 (size=9) ===

void FUN_1400042d0(longlong param_1)

{
  FUN_1400042b0(param_1 + -8);
  return;
}



// === FUN_1400042e0 @ 1400042e0 (size=83) ===

void FUN_1400042e0(longlong param_1,undefined4 param_2)

{
  int iVar1;
  undefined8 *puVar2;
  
  puVar2 = *(undefined8 **)(param_1 + 0x60);
  iVar1 = 0;
  while ((puVar2 != (undefined8 *)(param_1 + 0x60) && (-1 < iVar1))) {
    if (puVar2[0x45] != 0) {
      iVar1 = (*(code *)PTR__guard_dispatch_icall_140008188)(puVar2[0x45],param_2);
    }
    puVar2 = (undefined8 *)*puVar2;
  }
  return;
}



// === FUN_140004340 @ 140004340 (size=18) ===

/* WARNING: Switch with 1 destination removed at 0x00014000434b */

void FUN_140004340(longlong param_1)

{
                    /* WARNING: Could not recover jumptable at 0x000140007650. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (**(code **)(**(longlong **)(param_1 + 0x20) + 0x10))();
  return;
}



// === FUN_140004360 @ 140004360 (size=9) ===

void FUN_140004360(longlong param_1)

{
  FUN_140004340(param_1 + -8);
  return;
}



// === FUN_14000436c @ 14000436c (size=205) ===

undefined8 FUN_14000436c(undefined4 param_1,longlong param_2,longlong param_3,undefined4 *param_4)

{
  undefined8 uVar1;
  undefined4 local_res10 [2];
  undefined1 local_98 [16];
  longlong local_88;
  undefined4 local_80;
  longlong local_78;
  undefined4 *local_70;
  undefined4 local_68;
  undefined4 *local_60;
  undefined4 local_58;
  
  if (((param_2 == 0) || (param_3 == 0)) || (param_4 == (undefined4 *)0x0)) {
    uVar1 = 0xc000000d;
  }
  else {
    FUN_140007940(&local_88,0,(undefined1 *)0x70);
    local_res10[0] = *param_4;
    local_68 = 4;
    local_60 = local_res10;
    local_58 = 4;
    local_80 = 0x20;
    local_78 = param_3;
    local_70 = param_4;
    RtlInitUnicodeString(local_98,L"RtlQueryRegistryValuesEx");
    MmGetSystemRoutineAddress(local_98);
    uVar1 = (*(code *)PTR__guard_dispatch_icall_140008188)(param_1,param_2,&local_88,0,0);
  }
  return uVar1;
}



// === FUN_140004440 @ 140004440 (size=56) ===

undefined8 FUN_140004440(longlong param_1)

{
  undefined8 uVar1;
  
  uVar1 = 0;
  if (*(longlong *)(param_1 + 0x58) != 0) {
    uVar1 = (*(code *)PTR__guard_dispatch_icall_140008188)();
  }
  return uVar1;
}



// === FUN_140004480 @ 140004480 (size=23) ===

undefined4 FUN_140004480(longlong param_1)

{
  if (*(longlong *)(param_1 + 0x50) == 0) {
    return 0;
  }
  return *(undefined4 *)(*(longlong *)(param_1 + 0x50) + 0xf4);
}



// === FUN_140004490 @ 140004490 (size=23) ===

void FUN_140004490(longlong param_1,undefined4 param_2)

{
  if (*(longlong *)(param_1 + 0x50) != 0) {
    FUN_140004570(*(longlong *)(param_1 + 0x50),param_2);
  }
  return;
}



// === FUN_1400044b0 @ 1400044b0 (size=23) ===

undefined4 FUN_1400044b0(longlong param_1)

{
  if (*(longlong *)(param_1 + 0x50) == 0) {
    return 0;
  }
  return *(undefined4 *)(*(longlong *)(param_1 + 0x50) + 0xf8);
}



// === FUN_1400044c0 @ 1400044c0 (size=23) ===

void FUN_1400044c0(longlong param_1,undefined4 param_2)

{
  if (*(longlong *)(param_1 + 0x50) != 0) {
    FUN_140004580(*(longlong *)(param_1 + 0x50),param_2);
  }
  return;
}



// === FUN_1400044e0 @ 1400044e0 (size=23) ===

undefined4 FUN_1400044e0(longlong param_1)

{
  if (*(longlong *)(param_1 + 0x50) == 0) {
    return 0;
  }
  return *(undefined4 *)(*(longlong *)(param_1 + 0x50) + 0xfc);
}



// === FUN_1400044f0 @ 1400044f0 (size=23) ===

void FUN_1400044f0(longlong param_1,undefined4 param_2)

{
  if (*(longlong *)(param_1 + 0x50) != 0) {
    FUN_140004590(*(longlong *)(param_1 + 0x50),param_2);
  }
  return;
}



// === FUN_140004544 @ 140004544 (size=12) ===

void FUN_140004544(longlong param_1,uint param_2,undefined8 param_3,undefined4 param_4)

{
  if (param_2 < 0x14) {
    *(undefined4 *)(param_1 + (ulonglong)param_2 * 4) = param_4;
  }
  return;
}



// === FUN_140004550 @ 140004550 (size=7) ===

void FUN_140004550(longlong param_1,undefined4 param_2)

{
  *(undefined4 *)(param_1 + 0xf0) = param_2;
  return;
}



// === FUN_140004558 @ 140004558 (size=13) ===

void FUN_140004558(longlong param_1,uint param_2,undefined8 param_3,undefined4 param_4)

{
  if (param_2 < 0x14) {
    *(undefined4 *)(param_1 + 0x50 + (ulonglong)param_2 * 4) = param_4;
  }
  return;
}



// === FUN_140004570 @ 140004570 (size=7) ===

void FUN_140004570(longlong param_1,undefined4 param_2)

{
  *(undefined4 *)(param_1 + 0xf4) = param_2;
  return;
}



// === FUN_140004580 @ 140004580 (size=7) ===

void FUN_140004580(longlong param_1,undefined4 param_2)

{
  *(undefined4 *)(param_1 + 0xf8) = param_2;
  return;
}



// === FUN_140004590 @ 140004590 (size=7) ===

void FUN_140004590(longlong param_1,undefined4 param_2)

{
  *(undefined4 *)(param_1 + 0xfc) = param_2;
  return;
}



// === FUN_140004598 @ 140004598 (size=201) ===

undefined8
FUN_140004598(longlong param_1,undefined8 *param_2,undefined8 *param_3,ulonglong *param_4)

{
  undefined1 uVar1;
  ulonglong uVar2;
  
  uVar1 = KeAcquireSpinLockRaiseToDpc(param_1 + 0x168);
  uVar2 = *(ulonglong *)(param_1 + 0x188);
  if (uVar2 == 0) {
    uVar2 = KeQueryPerformanceCounter(0);
  }
  if ((*(int *)(param_1 + 0xbc) == 3) && (*(int *)(param_1 + 0xb8) == 0)) {
    FUN_140006320(param_1,uVar2,3);
  }
  if (param_2 != (undefined8 *)0x0) {
    *param_2 = *(undefined8 *)(param_1 + 0xe0);
  }
  if (param_3 != (undefined8 *)0x0) {
    *param_3 = *(undefined8 *)(param_1 + 0xe8);
  }
  if (param_4 != (ulonglong *)0x0) {
    *param_4 = uVar2;
  }
  KeReleaseSpinLock(param_1 + 0x168,uVar1);
  return 0;
}



// === FUN_140004664 @ 140004664 (size=155) ===

undefined8 FUN_140004664(longlong param_1,ulonglong *param_2)

{
  undefined8 uVar1;
  undefined8 uVar2;
  ulonglong uVar3;
  longlong local_res8 [2];
  undefined8 local_res18;
  ulonglong local_res20;
  
  local_res18 = 0;
  local_res8[0] = 0;
  uVar1 = *(undefined8 *)(*(longlong *)(param_1 + 0x98) + 0x128);
  uVar2 = FUN_140004598(param_1,&local_res18,local_res8,&local_res20);
  if (-1 < (int)uVar2) {
    uVar3 = ((ulonglong)*(uint *)(*(longlong *)(param_1 + 0x148) + 4) * local_res8[0]) /
            (ulonglong)*(uint *)(*(longlong *)(param_1 + 0x148) + 8);
    *param_2 = uVar3;
    param_2[1] = local_res20;
    (*(code *)PTR__guard_dispatch_icall_140008188)
              (uVar1,5,local_res18,*(undefined4 *)(param_1 + 0x7c),uVar3,0);
    uVar2 = 0;
  }
  return uVar2;
}



// === FUN_140004700 @ 140004700 (size=100) ===

ulonglong FUN_140004700(longlong param_1,uint param_2)

{
  undefined1 uVar1;
  ulonglong uVar2;
  
  if (param_2 == 0) {
    uVar2 = 0xc0000010;
  }
  else {
    uVar1 = KeAcquireSpinLockRaiseToDpc(param_1 + 0x168);
    uVar2 = FUN_140004764(param_1,param_2);
    uVar2 = uVar2 & 0xffffffff;
    KeReleaseSpinLock(param_1 + 0x168,uVar1);
  }
  return uVar2;
}



// === FUN_140004764 @ 140004764 (size=213) ===

undefined8 FUN_140004764(longlong param_1,uint param_2)

{
  undefined8 uVar1;
  
  if ((*(char *)(param_1 + 0x164) == '\0') && (param_2 <= *(uint *)(param_1 + 0xa8))) {
    uVar1 = *(undefined8 *)(*(longlong *)(param_1 + 0x98) + 0x128);
    (*(code *)PTR__guard_dispatch_icall_140008188)
              (uVar1,4,*(undefined8 *)(param_1 + 0xe0),*(undefined4 *)(param_1 + 0x7c),param_2,0);
    if ((*(int *)(param_1 + 0x68) != 0) && (*(uint *)(param_1 + 0x7c) == param_2)) {
      (*(code *)PTR__guard_dispatch_icall_140008188)
                (uVar1,7,*(undefined8 *)(param_1 + 0xe0),*(uint *)(param_1 + 0x7c),3,param_2);
    }
    *(uint *)(param_1 + 0x7c) = param_2;
    LOCK();
    *(undefined4 *)(param_1 + 0x80) = 1;
    UNLOCK();
    uVar1 = 0;
  }
  else {
    uVar1 = 0xc0000010;
  }
  return uVar1;
}



// === FUN_140004840 @ 140004840 (size=103) ===

ulonglong FUN_140004840(longlong param_1,uint param_2)

{
  undefined1 uVar1;
  ulonglong uVar2;
  
  uVar1 = KeAcquireSpinLockRaiseToDpc(param_1 + 0x140);
  uVar2 = FUN_140004764(param_1 + -0x28,param_2);
  if (-1 < (int)uVar2) {
    *(undefined1 *)(param_1 + 0x13c) = 1;
  }
  KeReleaseSpinLock(param_1 + 0x140,uVar1);
  return uVar2 & 0xffffffff;
}



// === FUN_1400048a8 @ 1400048a8 (size=14) ===

void FUN_1400048a8(undefined1 *param_1,undefined8 param_2)

{
  FUN_140003a04(param_2,param_1);
  return;
}



// === FUN_1400048b8 @ 1400048b8 (size=24) ===

void FUN_1400048b8(longlong param_1)

{
  if (param_1 != 0) {
    FUN_140003a54(param_1);
  }
  return;
}



// === FUN_1400048d0 @ 1400048d0 (size=134) ===

undefined8 *
FUN_1400048d0(undefined8 *param_1,undefined4 param_2,undefined8 *param_3,undefined8 param_4,
             undefined2 param_5,undefined4 param_6,undefined8 param_7)

{
  FUN_140018a70((undefined4 *)(param_1 + 4),param_2,param_4,param_5);
  FUN_140006e00(param_1 + 1,param_3);
  *param_1 = &PTR_FUN_140008ee8;
  param_1[1] = &PTR_FUN_140008f18;
  *(undefined4 *)(param_1 + 9) = param_6;
  FUN_140007940(param_1 + 10,0,(undefined1 *)0x204);
  *(undefined4 *)((longlong)param_1 + 0x4c) = 0;
  param_1[0x4b] = param_7;
  return param_1;
}



// === FUN_140004960 @ 140004960 (size=9) ===

void FUN_140004960(longlong param_1,uint param_2)

{
  FUN_14000496c((undefined8 *)(param_1 + -8),param_2);
  return;
}



// === FUN_14000496c @ 14000496c (size=52) ===

undefined8 * FUN_14000496c(undefined8 *param_1,uint param_2)

{
  FUN_140018a8c(param_1);
  if ((param_2 & 1) != 0) {
    FUN_1400048b8((longlong)param_1);
  }
  return param_1;
}



// === FUN_1400049a0 @ 1400049a0 (size=18) ===

/* WARNING: Switch with 1 destination removed at 0x0001400049ab */

void FUN_1400049a0(longlong param_1)

{
                    /* WARNING: Could not recover jumptable at 0x000140007650. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (**(code **)(**(longlong **)(param_1 + 0x18) + 8))();
  return;
}



// === FUN_1400049c0 @ 1400049c0 (size=17) ===

/* WARNING: Switch with 1 destination removed at 0x0001400049ca */

void FUN_1400049c0(longlong param_1)

{
                    /* WARNING: Could not recover jumptable at 0x000140007650. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (**(code **)**(undefined8 **)(param_1 + 0x18))();
  return;
}



// === FUN_1400049e0 @ 1400049e0 (size=18) ===

/* WARNING: Switch with 1 destination removed at 0x0001400049eb */

void FUN_1400049e0(longlong param_1)

{
                    /* WARNING: Could not recover jumptable at 0x000140007650. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (**(code **)(**(longlong **)(param_1 + 0x18) + 0x10))();
  return;
}



// === FUN_1400049f4 @ 1400049f4 (size=112) ===

undefined8 * FUN_1400049f4(undefined8 *param_1,undefined8 *param_2)

{
  FUN_140006e00(param_1 + 6,param_2);
  *param_1 = &PTR_FUN_140009050;
  param_1[1] = &PTR_FUN_140009070;
  param_1[2] = &PTR_FUN_1400090e8;
  param_1[3] = &PTR_FUN_140009108;
  param_1[4] = &PTR_FUN_140009138;
  param_1[5] = &PTR_FUN_1400091b8;
  param_1[6] = &PTR_FUN_1400091d8;
  return param_1;
}



// === FUN_140004a70 @ 140004a70 (size=9) ===

void FUN_140004a70(longlong param_1,uint param_2)

{
  FUN_140004a8c((undefined8 *)(param_1 + -0x18),param_2);
  return;
}



// === FUN_140004a80 @ 140004a80 (size=9) ===

void FUN_140004a80(longlong param_1,uint param_2)

{
  FUN_140004ac0((undefined8 *)(param_1 + -0x30),param_2);
  return;
}



// === FUN_140004a8c @ 140004a8c (size=52) ===

undefined8 * FUN_140004a8c(undefined8 *param_1,uint param_2)

{
  FUN_1400191e0(param_1);
  if ((param_2 & 1) != 0) {
    FUN_1400048b8((longlong)param_1);
  }
  return param_1;
}



// === FUN_140004ac0 @ 140004ac0 (size=52) ===

undefined8 * FUN_140004ac0(undefined8 *param_1,uint param_2)

{
  FUN_14001a53c(param_1);
  if ((param_2 & 1) != 0) {
    FUN_1400048b8((longlong)param_1);
  }
  return param_1;
}



// === FUN_140004af4 @ 140004af4 (size=34) ===

void FUN_140004af4(longlong param_1)

{
  undefined1 uVar1;
  
  uVar1 = KeAcquireSpinLockRaiseToDpc(param_1 + 0xe0);
  *(undefined1 *)(param_1 + 0xe8) = uVar1;
  return;
}



// === FUN_140004b20 @ 140004b20 (size=18) ===

/* WARNING: Switch with 1 destination removed at 0x000140004b2b */

void FUN_140004b20(longlong param_1)

{
                    /* WARNING: Could not recover jumptable at 0x000140007650. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (**(code **)(**(longlong **)(param_1 + 0x28) + 8))();
  return;
}



// === FUN_140004b40 @ 140004b40 (size=9) ===

void FUN_140004b40(longlong param_1)

{
  FUN_140004b20(param_1 + -8);
  return;
}



// === FUN_140004b50 @ 140004b50 (size=9) ===

void FUN_140004b50(longlong param_1)

{
  FUN_140004b20(param_1 + -0x10);
  return;
}



// === FUN_140004b60 @ 140004b60 (size=18) ===

/* WARNING: Switch with 1 destination removed at 0x000140004b6b */

void FUN_140004b60(longlong param_1)

{
                    /* WARNING: Could not recover jumptable at 0x000140007650. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (**(code **)(**(longlong **)(param_1 + 0x40) + 8))();
  return;
}



// === FUN_140004b80 @ 140004b80 (size=9) ===

void FUN_140004b80(longlong param_1)

{
  FUN_140004b60(param_1 + -8);
  return;
}



// === FUN_140004b90 @ 140004b90 (size=9) ===

void FUN_140004b90(longlong param_1)

{
  FUN_140004b60(param_1 + -0x10);
  return;
}



// === FUN_140004ba0 @ 140004ba0 (size=9) ===

void FUN_140004ba0(longlong param_1)

{
  FUN_140004b60(param_1 + -0x18);
  return;
}



// === FUN_140004bb0 @ 140004bb0 (size=9) ===

void FUN_140004bb0(longlong param_1)

{
  FUN_140004b60(param_1 + -0x20);
  return;
}



// === FUN_140004bc0 @ 140004bc0 (size=9) ===

void FUN_140004bc0(longlong param_1)

{
  FUN_140004b60(param_1 + -0x28);
  return;
}



// === FUN_140004bcc @ 140004bcc (size=118) ===

undefined4 FUN_140004bcc(longlong param_1,undefined8 *param_2)

{
  longlong lVar1;
  ulonglong uVar2;
  
  FUN_140004af4(param_1);
  lVar1 = *(longlong *)(param_1 + 0xd8);
  uVar2 = (ulonglong)(*(int *)(param_1 + 0xec) - 1);
  if (param_2 != (undefined8 *)0x0) {
    *param_2 = *(undefined8 *)(lVar1 + 8 + uVar2 * 0x28);
  }
  KeReleaseSpinLock(param_1 + 0xe0,*(undefined1 *)(param_1 + 0xe8));
  return *(undefined4 *)(lVar1 + 0x10 + uVar2 * 0x28);
}



// === FUN_140004c44 @ 140004c44 (size=103) ===

int FUN_140004c44(longlong param_1,uint param_2,undefined8 *param_3)

{
  int iVar1;
  
  FUN_140004af4(param_1);
  iVar1 = *(int *)(*(longlong *)(param_1 + 0xd8) + 0x20 + (ulonglong)param_2 * 0x28);
  if (param_3 != (undefined8 *)0x0) {
    if (iVar1 == 0) {
      *param_3 = 0;
    }
    else {
      *param_3 = *(undefined8 *)(*(longlong *)(param_1 + 0xd8) + 0x18 + (ulonglong)param_2 * 0x28);
    }
  }
  KeReleaseSpinLock(param_1 + 0xe0,*(undefined1 *)(param_1 + 0xe8));
  return iVar1;
}



// === FUN_140004cac @ 140004cac (size=72) ===

bool FUN_140004cac(longlong param_1,uint param_2)

{
  int iVar1;
  
  FUN_140004af4(param_1);
  iVar1 = *(int *)(*(longlong *)(param_1 + 0xd8) + (ulonglong)param_2 * 0x28);
  KeReleaseSpinLock(param_1 + 0xe0,
                    CONCAT71((int7)((ulonglong)param_2 * 5 >> 8),*(undefined1 *)(param_1 + 0xe8)));
  return iVar1 == 1;
}



// === FUN_140004cf4 @ 140004cf4 (size=72) ===

bool FUN_140004cf4(longlong param_1,uint param_2)

{
  int iVar1;
  
  FUN_140004af4(param_1);
  iVar1 = *(int *)(*(longlong *)(param_1 + 0xd8) + (ulonglong)param_2 * 0x28);
  KeReleaseSpinLock(param_1 + 0xe0,
                    CONCAT71((int7)((ulonglong)param_2 * 5 >> 8),*(undefined1 *)(param_1 + 0xe8)));
  return iVar1 == 3;
}



// === FUN_140004d3c @ 140004d3c (size=72) ===

bool FUN_140004d3c(longlong param_1,uint param_2)

{
  int iVar1;
  
  FUN_140004af4(param_1);
  iVar1 = *(int *)(*(longlong *)(param_1 + 0xd8) + (ulonglong)param_2 * 0x28);
  KeReleaseSpinLock(param_1 + 0xe0,
                    CONCAT71((int7)((ulonglong)param_2 * 5 >> 8),*(undefined1 *)(param_1 + 0xe8)));
  return iVar1 == 4;
}



// === FUN_140004d84 @ 140004d84 (size=72) ===

bool FUN_140004d84(longlong param_1,uint param_2)

{
  int iVar1;
  
  FUN_140004af4(param_1);
  iVar1 = *(int *)(*(longlong *)(param_1 + 0xd8) + (ulonglong)param_2 * 0x28);
  KeReleaseSpinLock(param_1 + 0xe0,
                    CONCAT71((int7)((ulonglong)param_2 * 5 >> 8),*(undefined1 *)(param_1 + 0xe8)));
  return iVar1 == 2;
}



// === FUN_140004dd0 @ 140004dd0 (size=17) ===

/* WARNING: Switch with 1 destination removed at 0x000140004dda */

void FUN_140004dd0(longlong param_1)

{
                    /* WARNING: Could not recover jumptable at 0x000140007650. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (**(code **)**(undefined8 **)(param_1 + 0x28))();
  return;
}



// === FUN_140004df0 @ 140004df0 (size=9) ===

void FUN_140004df0(longlong param_1)

{
  FUN_140004dd0(param_1 + -8);
  return;
}



// === FUN_140004e00 @ 140004e00 (size=9) ===

void FUN_140004e00(longlong param_1)

{
  FUN_140004dd0(param_1 + -0x10);
  return;
}



// === FUN_140004e10 @ 140004e10 (size=17) ===

/* WARNING: Switch with 1 destination removed at 0x000140004e1a */

void FUN_140004e10(longlong param_1)

{
                    /* WARNING: Could not recover jumptable at 0x000140007650. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (**(code **)**(undefined8 **)(param_1 + 0x40))();
  return;
}



// === FUN_140004e30 @ 140004e30 (size=9) ===

void FUN_140004e30(longlong param_1)

{
  FUN_140004e10(param_1 + -8);
  return;
}



// === FUN_140004e40 @ 140004e40 (size=9) ===

void FUN_140004e40(longlong param_1)

{
  FUN_140004e10(param_1 + -0x10);
  return;
}



// === FUN_140004e50 @ 140004e50 (size=9) ===

void FUN_140004e50(longlong param_1)

{
  FUN_140004e10(param_1 + -0x18);
  return;
}



// === FUN_140004e60 @ 140004e60 (size=9) ===

void FUN_140004e60(longlong param_1)

{
  FUN_140004e10(param_1 + -0x20);
  return;
}



// === FUN_140004e70 @ 140004e70 (size=9) ===

void FUN_140004e70(longlong param_1)

{
  FUN_140004e10(param_1 + -0x28);
  return;
}



// === FUN_140004e80 @ 140004e80 (size=18) ===

/* WARNING: Switch with 1 destination removed at 0x000140004e8b */

void FUN_140004e80(longlong param_1)

{
                    /* WARNING: Could not recover jumptable at 0x000140007650. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (**(code **)(**(longlong **)(param_1 + 0x28) + 0x10))();
  return;
}



// === FUN_140004ea0 @ 140004ea0 (size=9) ===

void FUN_140004ea0(longlong param_1)

{
  FUN_140004e80(param_1 + -8);
  return;
}



// === FUN_140004eb0 @ 140004eb0 (size=9) ===

void FUN_140004eb0(longlong param_1)

{
  FUN_140004e80(param_1 + -0x10);
  return;
}



// === FUN_140004ec0 @ 140004ec0 (size=18) ===

/* WARNING: Switch with 1 destination removed at 0x000140004ecb */

void FUN_140004ec0(longlong param_1)

{
                    /* WARNING: Could not recover jumptable at 0x000140007650. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (**(code **)(**(longlong **)(param_1 + 0x40) + 0x10))();
  return;
}



// === FUN_140004ee0 @ 140004ee0 (size=9) ===

void FUN_140004ee0(longlong param_1)

{
  FUN_140004ec0(param_1 + -8);
  return;
}



// === FUN_140004ef0 @ 140004ef0 (size=9) ===

void FUN_140004ef0(longlong param_1)

{
  FUN_140004ec0(param_1 + -0x10);
  return;
}



// === FUN_140004f00 @ 140004f00 (size=9) ===

void FUN_140004f00(longlong param_1)

{
  FUN_140004ec0(param_1 + -0x18);
  return;
}



// === FUN_140004f10 @ 140004f10 (size=9) ===

void FUN_140004f10(longlong param_1)

{
  FUN_140004ec0(param_1 + -0x20);
  return;
}



// === FUN_140004f20 @ 140004f20 (size=9) ===

void FUN_140004f20(longlong param_1)

{
  FUN_140004ec0(param_1 + -0x28);
  return;
}



// === FUN_140004f2c @ 140004f2c (size=635) ===

/* WARNING: Function: __security_check_cookie replaced with injection: security_check_cookie */

void FUN_140004f2c(longlong param_1,longlong param_2,int param_3,int param_4,int param_5,int param_6
                  )

{
  uint uVar1;
  int iVar2;
  int iVar3;
  int *piVar4;
  longlong lVar5;
  uint uVar6;
  longlong lVar7;
  longlong lVar8;
  ulonglong uVar9;
  ulonglong uVar10;
  short *psVar11;
  longlong lVar12;
  int aiStack_178 [32];
  int aiStack_f8 [10];
  undefined8 uStack_d0;
  undefined1 auStack_c8 [32];
  longlong local_a8 [16];
  ulonglong local_28;
  
  local_28 = DAT_140012be0 ^ (ulonglong)auStack_c8;
  lVar12 = (longlong)param_4;
  iVar2 = 0x20;
  if (param_4 < 0x21) {
    iVar2 = param_4;
  }
  lVar7 = 0;
  lVar8 = (longlong)iVar2;
  if (0 < iVar2) {
    uStack_d0 = 0x140004f8d;
    FUN_140007940(local_a8,0,(undefined1 *)(lVar8 << 2));
  }
  if (param_5 == 8) {
    uVar1 = (uint)((longlong)param_3 / (longlong)param_4);
    uVar10 = (longlong)param_3 / (longlong)param_4 & 0xffffffff;
    while (0 < (int)uVar1) {
      lVar5 = lVar7;
      if (0 < lVar8) {
        do {
          uVar1 = (*(byte *)(lVar5 + param_2) - 0x80) * 0x10000;
          uVar6 = (int)uVar1 >> 0x1f;
          iVar2 = (uVar1 ^ uVar6) - uVar6;
          if (*(int *)((longlong)local_a8 + lVar5 * 4) < iVar2) {
            *(int *)((longlong)local_a8 + lVar5 * 4) = iVar2;
          }
          lVar5 = lVar5 + 1;
        } while (lVar5 < lVar8);
      }
      param_2 = param_2 + lVar12;
      uVar1 = (int)uVar10 - 1;
      uVar10 = (ulonglong)uVar1;
    }
  }
  else if (param_5 == 0x10) {
    uVar10 = (longlong)param_3 / (longlong)(param_4 * 2);
    uVar9 = uVar10 & 0xffffffff;
    if (0 < (int)uVar10) {
      do {
        lVar5 = lVar7;
        if (0 < lVar8) {
          do {
            uVar1 = (int)*(short *)(param_2 + lVar5 * 2) << 8;
            uVar6 = (int)uVar1 >> 0x1f;
            iVar2 = (uVar1 ^ uVar6) - uVar6;
            if (*(int *)((longlong)local_a8 + lVar5 * 4) < iVar2) {
              *(int *)((longlong)local_a8 + lVar5 * 4) = iVar2;
            }
            lVar5 = lVar5 + 1;
          } while (lVar5 < lVar8);
        }
        param_2 = param_2 + lVar12 * 2;
        uVar1 = (int)uVar9 - 1;
        uVar9 = (ulonglong)uVar1;
      } while (0 < (int)uVar1);
    }
  }
  else if (param_5 == 0x18) {
    uVar10 = (longlong)param_3 / (longlong)(param_4 * 3);
    uVar9 = uVar10 & 0xffffffff;
    if (0 < (int)uVar10) {
      do {
        if (0 < lVar8) {
          psVar11 = (short *)(param_2 + 1);
          lVar12 = lVar7;
          do {
            iVar3 = (int)((uint)CONCAT21(*psVar11,*(undefined1 *)((longlong)psVar11 + -1)) << 8) >>
                    8;
            iVar2 = -iVar3;
            if (-1 < *psVar11) {
              iVar2 = iVar3;
            }
            if (*(int *)((longlong)local_a8 + lVar12 * 4) < iVar2) {
              *(int *)((longlong)local_a8 + lVar12 * 4) = iVar2;
            }
            psVar11 = (short *)((longlong)psVar11 + 3);
            lVar12 = lVar12 + 1;
          } while (lVar12 < lVar8);
        }
        param_2 = param_2 + param_4 * 3;
        uVar1 = (int)uVar9 - 1;
        uVar9 = (ulonglong)uVar1;
      } while (0 < (int)uVar1);
    }
  }
  else if ((param_5 == 0x78c) &&
          (uVar10 = (longlong)param_3 / (longlong)(param_4 * 4), uVar9 = uVar10 & 0xffffffff,
          0 < (int)uVar10)) {
    do {
      lVar5 = lVar7;
      if (0 < lVar8) {
        do {
          uVar1 = *(int *)(param_2 + lVar5 * 4) << 5;
          uVar6 = (int)uVar1 >> 0x1f;
          iVar2 = (uVar1 ^ uVar6) - uVar6;
          if (*(int *)((longlong)local_a8 + lVar5 * 4) < iVar2) {
            *(int *)((longlong)local_a8 + lVar5 * 4) = iVar2;
          }
          lVar5 = lVar5 + 1;
        } while (lVar5 < lVar8);
      }
      param_2 = param_2 + lVar12 * 4;
      uVar1 = (int)uVar9 - 1;
      uVar9 = (ulonglong)uVar1;
    } while (0 < (int)uVar1);
  }
  if (0 < lVar8) {
    if (param_6 == 0) {
      piVar4 = (int *)(param_1 + 0x50);
      do {
        iVar2 = *(int *)((longlong)aiStack_f8 + -param_1 + (longlong)piVar4);
        if (iVar2 < *piVar4) {
          iVar2 = *piVar4 * 0x7f >> 7;
        }
        *piVar4 = iVar2;
        lVar7 = lVar7 + 1;
        piVar4 = piVar4 + 1;
      } while (lVar7 < lVar8);
    }
    else {
      piVar4 = (int *)(param_1 + 0xd0);
      do {
        iVar2 = *(int *)((longlong)aiStack_178 + -param_1 + (longlong)piVar4);
        if (iVar2 < *piVar4) {
          iVar2 = *piVar4 * 0x7f >> 7;
        }
        *piVar4 = iVar2;
        lVar7 = lVar7 + 1;
        piVar4 = piVar4 + 1;
      } while (lVar7 < lVar8);
    }
  }
  return;
}



// === FUN_1400051a8 @ 1400051a8 (size=456) ===

void FUN_1400051a8(byte *param_1,int param_2,int param_3,int param_4,int *param_5)

{
  int iVar1;
  int iVar2;
  ulonglong uVar3;
  longlong lVar4;
  byte *pbVar5;
  longlong lVar6;
  ulonglong uVar7;
  int *piVar8;
  uint uVar9;
  longlong lVar10;
  
  iVar1 = 0x10;
  if (param_3 < 0x11) {
    iVar1 = param_3;
  }
  lVar10 = (longlong)iVar1;
  if (param_4 == 8) {
    uVar3 = (longlong)param_2 / (longlong)param_3 & 0xffffffff;
    if (0 < (int)((longlong)param_2 / (longlong)param_3)) {
      do {
        pbVar5 = param_1;
        piVar8 = param_5;
        lVar4 = lVar10;
        if (0 < lVar10) {
          do {
            *pbVar5 = (char)((int)((*pbVar5 - 0x80) * *piVar8 * 0x10000) >> 0x17) + 0x80;
            lVar4 = lVar4 + -1;
            pbVar5 = pbVar5 + 1;
            piVar8 = piVar8 + 1;
          } while (lVar4 != 0);
        }
        param_1 = param_1 + param_3;
        uVar9 = (int)uVar3 - 1;
        uVar3 = (ulonglong)uVar9;
      } while (0 < (int)uVar9);
    }
  }
  else if (param_4 == 0x10) {
    uVar3 = (longlong)param_2 / (longlong)(param_3 * 2);
    uVar7 = uVar3 & 0xffffffff;
    if (0 < (int)uVar3) {
      do {
        lVar4 = 0;
        if (0 < lVar10) {
          do {
            *(short *)(param_1 + lVar4 * 2) =
                 (short)(*(short *)(param_1 + lVar4 * 2) * 0x100 * param_5[lVar4] >> 0xf);
            lVar4 = lVar4 + 1;
          } while (lVar4 < lVar10);
        }
        param_1 = param_1 + (longlong)param_3 * 2;
        uVar9 = (int)uVar7 - 1;
        uVar7 = (ulonglong)uVar9;
      } while (0 < (int)uVar9);
    }
  }
  else if (param_4 == 0x18) {
    uVar3 = (longlong)param_2 / (longlong)(param_3 * 3);
    uVar7 = uVar3 & 0xffffffff;
    if (0 < (int)uVar3) {
      do {
        lVar4 = 0;
        if (0 < lVar10) {
          pbVar5 = param_1 + 2;
          do {
            iVar1 = ((int)((uint)CONCAT21(CONCAT11(*pbVar5,pbVar5[-1]),pbVar5[-2]) << 8) >> 8) *
                    param_5[lVar4];
            lVar4 = lVar4 + 1;
            iVar2 = iVar1 >> 7;
            pbVar5[-2] = (byte)iVar2;
            pbVar5[-1] = (byte)((uint)iVar2 >> 8);
            *pbVar5 = (byte)((uint)(iVar1 >> 0xf) >> 8);
            pbVar5 = pbVar5 + 3;
          } while (lVar4 < lVar10);
        }
        param_1 = param_1 + param_3 * 3;
        uVar9 = (int)uVar7 - 1;
        uVar7 = (ulonglong)uVar9;
      } while (0 < (int)uVar9);
    }
  }
  else if ((param_4 == 0x78c) &&
          (uVar3 = (longlong)param_2 / (longlong)(param_3 * 4), uVar7 = uVar3 & 0xffffffff,
          0 < (int)uVar3)) {
    lVar4 = (longlong)param_5 - (longlong)param_1;
    do {
      lVar6 = lVar10;
      pbVar5 = param_1;
      if (0 < lVar10) {
        do {
          *(int *)pbVar5 = *(int *)(pbVar5 + lVar4) * *(int *)pbVar5 >> 7;
          lVar6 = lVar6 + -1;
          pbVar5 = pbVar5 + 4;
        } while (lVar6 != 0);
      }
      param_1 = param_1 + (longlong)param_3 * 4;
      lVar4 = lVar4 + (longlong)param_3 * -4;
      uVar9 = (int)uVar7 - 1;
      uVar7 = (ulonglong)uVar9;
    } while (0 < (int)uVar9);
  }
  return;
}



// === FUN_140005370 @ 140005370 (size=48) ===

void FUN_140005370(int param_1,int param_2)

{
  undefined *puVar1;
  uint uVar2;
  
  puVar1 = FUN_140004080(param_1);
  if (puVar1 != (undefined *)0x0) {
    if (param_2 == 0) {
      uVar2 = *(uint *)(puVar1 + 0x174) & 0xfffffffd;
    }
    else {
      uVar2 = *(uint *)(puVar1 + 0x174) | 2;
    }
    *(uint *)(puVar1 + 0x174) = uVar2;
  }
  return;
}



// === FUN_1400053a0 @ 1400053a0 (size=24) ===

undefined8 FUN_1400053a0(longlong param_1,ulonglong *param_2)

{
  undefined8 uVar1;
  
  if (*(int *)(param_1 + 0xa0) == 0) {
    return 0xc00000bb;
  }
  uVar1 = FUN_140004664(param_1 + -0x18,param_2);
  return uVar1;
}



// === FUN_1400053c0 @ 1400053c0 (size=90) ===

undefined8 FUN_1400053c0(longlong param_1,undefined4 *param_2)

{
  undefined4 uVar1;
  undefined1 uVar2;
  undefined8 uVar3;
  
  if (*(int *)(param_1 + 0xa0) == 0) {
    uVar3 = 0xc00000bb;
  }
  else {
    uVar2 = KeAcquireSpinLockRaiseToDpc(param_1 + 0x150);
    uVar1 = *(undefined4 *)(param_1 + 0xe0);
    *param_2 = uVar1;
    KeReleaseSpinLock(param_1 + 0x150,CONCAT31((int3)((uint)uVar1 >> 8),uVar2));
    uVar3 = 0;
  }
  return uVar3;
}



// === FUN_140005420 @ 140005420 (size=245) ===

undefined8 FUN_140005420(longlong param_1,undefined8 *param_2)

{
  undefined1 uVar1;
  undefined *puVar2;
  ulonglong uVar3;
  
  uVar1 = KeAcquireSpinLockRaiseToDpc(param_1 + 0x160);
  puVar2 = FUN_140004080(*(int *)(*(longlong *)(param_1 + 0x90) + 0x120));
  if (puVar2 != (undefined *)0x0) {
    if (*(char *)(param_1 + 0x9c) == '\0') {
      *(int *)(puVar2 + 0x170) = *(int *)(puVar2 + 0x170) + 1;
    }
    else {
      *(int *)(puVar2 + 0x198) = *(int *)(puVar2 + 0x198) + 1;
    }
  }
  if ((*(int *)(param_1 + 0xb4) == 3) && (*(int *)(param_1 + 0xb0) == 0)) {
    uVar3 = *(ulonglong *)(param_1 + 0x180);
    if (uVar3 == 0) {
      uVar3 = KeQueryPerformanceCounter(0);
    }
    FUN_140006320(param_1 + -8,uVar3,1);
    if (puVar2 != (undefined *)0x0) {
      if (*(char *)(param_1 + 0x9c) == '\0') {
        *(int *)(puVar2 + 0x164) = *(int *)(puVar2 + 0x164) + 1;
      }
      else {
        *(int *)(puVar2 + 0x18c) = *(int *)(puVar2 + 0x18c) + 1;
      }
    }
  }
  *param_2 = *(undefined8 *)(param_1 + 200);
  param_2[1] = *(undefined8 *)(param_1 + 0xd0);
  KeReleaseSpinLock(param_1 + 0x160,uVar1);
  return 0;
}



// === FUN_140005520 @ 140005520 (size=274) ===

undefined8
FUN_140005520(longlong param_1,int *param_2,undefined4 *param_3,undefined8 *param_4,
             undefined4 *param_5)

{
  undefined8 uVar1;
  undefined1 uVar2;
  undefined8 uVar3;
  undefined *puVar4;
  int iVar5;
  
  if (*(int *)(param_1 + 0xa8) == 0) {
    uVar3 = 0xc00000bb;
  }
  else {
    *param_3 = 0;
    if (*(int *)(param_1 + 0xac) < 2) {
      uVar3 = 0xc0000184;
    }
    else {
      puVar4 = FUN_140004080(*(int *)(*(longlong *)(param_1 + 0x88) + 0x120));
      if (*(char *)(param_1 + 0x94) == '\0') {
        *(int *)(puVar4 + 0x16c) = *(int *)(puVar4 + 0x16c) + 1;
      }
      else {
        *(int *)(puVar4 + 0x194) = *(int *)(puVar4 + 0x194) + 1;
      }
      uVar2 = KeAcquireSpinLockRaiseToDpc(param_1 + 0x158);
      uVar3 = *(undefined8 *)(param_1 + 0xe8);
      uVar1 = *(undefined8 *)(param_1 + 0x1b8);
      KeReleaseSpinLock(param_1 + 0x158,uVar2);
      iVar5 = (int)uVar3 + -1;
      if (iVar5 == *(int *)(param_1 + 0xe0)) {
        uVar3 = 0xc00000a3;
      }
      else {
        if (iVar5 - *(int *)(param_1 + 0xe0) != 1) {
          if (*(char *)(param_1 + 0x94) == '\0') {
            *(int *)(puVar4 + 0x160) = *(int *)(puVar4 + 0x160) + 1;
          }
          else {
            *(int *)(puVar4 + 0x188) = *(int *)(puVar4 + 0x188) + 1;
          }
        }
        *param_2 = iVar5;
        *param_4 = uVar1;
        *param_3 = 0;
        *param_5 = 0;
        uVar3 = 0;
        *(int *)(param_1 + 0xe0) = iVar5;
      }
    }
  }
  return uVar3;
}



// === FUN_140005634 @ 140005634 (size=720) ===

/* WARNING: Function: __security_check_cookie replaced with injection: security_check_cookie */

void FUN_140005634(longlong param_1,uint param_2,uint param_3)

{
  int iVar1;
  int iVar2;
  undefined *puVar3;
  undefined8 uVar4;
  int *piVar5;
  int iVar6;
  uint uVar7;
  ulonglong uVar8;
  int iVar9;
  int aiStackY_c8 [8];
  
  iVar9 = 0;
  if ((*(longlong *)(param_1 + 0x178) != 0) &&
     (puVar3 = FUN_140004080(*(int *)(*(longlong *)(param_1 + 0x98) + 0x120)),
     puVar3 != (undefined *)0x0)) {
    uVar7 = *(int *)(param_1 + 0xa8) - param_3;
    if (param_2 < uVar7) {
      uVar7 = param_2;
    }
    FUN_140007680(*(undefined8 **)(param_1 + 0x178),
                  (undefined8 *)((ulonglong)param_3 + *(longlong *)(param_1 + 0xb0)),
                  (ulonglong)uVar7);
    if (uVar7 < param_2) {
      FUN_140007680((undefined8 *)(*(longlong *)(param_1 + 0x178) + (ulonglong)uVar7),
                    *(undefined8 **)(param_1 + 0xb0),(ulonglong)(param_2 - uVar7));
    }
    if (*(int *)(param_1 + 0xb8) == 0) {
      if (*(longlong *)(param_1 + 0x170) != 0) {
        *(undefined4 *)(*(longlong *)(param_1 + 0x170) + 4) =
             *(undefined4 *)(*(longlong *)(puVar3 + 0x1a0) + 4);
        FUN_1400022b0(*(int **)(param_1 + 0x170),*(undefined8 **)(param_1 + 0x178),param_2,
                      *(int *)(param_1 + 0x8c),(uint)*(ushort *)(param_1 + 0x88),
                      (uint)*(ushort *)(param_1 + 0x86),1);
      }
    }
    else {
      if ((*(uint *)(puVar3 + 0x174) & 2) != 0) {
        uVar7 = 0x10;
        if ((0x10 < *(ushort *)(param_1 + 0x88)) ||
           (uVar7 = (uint)*(ushort *)(param_1 + 0x88), uVar7 != 0)) {
          uVar8 = (ulonglong)uVar7;
          piVar5 = (int *)(puVar3 + 0x230);
          do {
            iVar6 = *(short *)((longlong)piVar5 + -0x7e) + 0x60;
            if (0x5f < iVar6) {
              iVar6 = 0x5f;
            }
            if (piVar5[-0x10] == 1) {
              iVar6 = 0;
            }
            else {
              iVar2 = 0;
              if (-1 < iVar6) {
                iVar2 = iVar6;
              }
              iVar6 = *(int *)(&DAT_140012a60 + (longlong)iVar2 * 4);
            }
            *(int *)(((longlong)aiStackY_c8 - (longlong)(puVar3 + 0x1f0)) + (longlong)piVar5) =
                 iVar6;
            iVar2 = *piVar5;
            iVar1 = iVar2 - iVar6;
            if (iVar2 < iVar6) {
              iVar2 = iVar2 + 1;
              *piVar5 = iVar2;
              iVar1 = iVar2 - iVar6;
            }
            if (iVar2 != iVar6 && SBORROW4(iVar2,iVar6) == iVar1 < 0) {
              *piVar5 = iVar2 + -1;
            }
            piVar5 = piVar5 + 1;
            uVar8 = uVar8 - 1;
          } while (uVar8 != 0);
          FUN_1400051a8(*(byte **)(param_1 + 0x178),param_2,(uint)*(ushort *)(param_1 + 0x88),
                        (uint)*(ushort *)(param_1 + 0x86),(int *)(puVar3 + 0x230));
        }
      }
      if ((DAT_140012f80 == 0) && (*(int **)(puVar3 + 0x1a0) != (int *)0x0)) {
        uVar4 = FUN_1400022b0(*(int **)(puVar3 + 0x1a0),*(undefined8 **)(param_1 + 0x178),param_2,
                              *(int *)(param_1 + 0x8c),(uint)*(ushort *)(param_1 + 0x88),
                              (uint)*(ushort *)(param_1 + 0x86),1);
        iVar9 = (int)uVar4;
      }
      FUN_140004f2c((longlong)puVar3,*(longlong *)(param_1 + 0x178),param_2,
                    (uint)*(ushort *)(param_1 + 0x88),(uint)*(ushort *)(param_1 + 0x86),0);
      if (iVar9 != 0) {
        *(int *)(puVar3 + 0x24) = *(int *)(puVar3 + 0x24) + 1;
      }
    }
    uVar7 = (uint)(*(ushort *)(param_1 + 0x86) >> 3) * (uint)*(ushort *)(param_1 + 0x88);
    if (uVar7 != 0) {
      uVar7 = param_2 / uVar7;
      if (uVar7 < 0x400) {
        if (uVar7 < 0x200) {
          if (uVar7 < 0x100) {
            *(int *)(puVar3 + 0x2c) = *(int *)(puVar3 + 0x2c) + 1;
          }
          else {
            *(int *)(puVar3 + 0x30) = *(int *)(puVar3 + 0x30) + 1;
          }
        }
        else {
          *(int *)(puVar3 + 0x34) = *(int *)(puVar3 + 0x34) + 1;
        }
      }
      else {
        *(int *)(puVar3 + 0x38) = *(int *)(puVar3 + 0x38) + 1;
      }
      if (*(uint *)(puVar3 + 0x19c) < uVar7 * 3) {
        *(int *)(puVar3 + 0xc0) = *(int *)(puVar3 + 0xc0) + 1;
      }
    }
    *(int *)(puVar3 + 0x4c) = *(int *)(puVar3 + 0x4c) + 1;
  }
  return;
}



// === FUN_140005910 @ 140005910 (size=564) ===

undefined8 FUN_140005910(longlong param_1,int param_2)

{
  ulonglong uVar1;
  bool bVar2;
  undefined1 uVar3;
  ulonglong uVar4;
  ulonglong uVar5;
  longlong lVar6;
  undefined7 extraout_var;
  undefined *puVar7;
  
  (*(code *)PTR__guard_dispatch_icall_140008188)
            (*(undefined8 *)(*(longlong *)(param_1 + 0x90) + 0x128),2,
             *(undefined8 *)(param_1 + 0xd8),*(undefined4 *)(param_1 + 0x74),(longlong)param_2,0);
  if (param_2 == 0) {
    uVar3 = KeAcquireSpinLockRaiseToDpc();
    *(undefined8 *)(param_1 + 0xf0) = 0;
    *(undefined4 *)(param_1 + 0xe8) = 0xffffffff;
    *(undefined4 *)(param_1 + 0xec) = 0xffffffff;
    *(undefined8 *)(param_1 + 200) = 0;
    *(undefined8 *)(param_1 + 0xd0) = 0;
    *(undefined8 *)(param_1 + 0xd8) = 0;
    *(undefined8 *)(param_1 + 0xe0) = 0;
    *(undefined4 *)(param_1 + 0x1b0) = 0;
    *(undefined4 *)(param_1 + 0x74) = 0;
    *(undefined2 *)(param_1 + 0x15c) = 0;
    KeReleaseSpinLock(param_1 + 0x160,uVar3);
  }
  else if (param_2 == 2) {
    if (2 < *(int *)(param_1 + 0xb4)) {
      if (*(longlong *)(param_1 + 0x58) != 0) {
        FUN_14000669c(param_1 + -8);
        *(undefined8 *)(param_1 + 0x58) = 0;
        KeFlushQueuedDpcs();
        if (*(longlong *)(param_1 + 0x168) != 0) {
          FUN_1400039ac(*(longlong *)(param_1 + 0x168));
        }
      }
      puVar7 = FUN_140004080(*(int *)(*(longlong *)(param_1 + 0x90) + 0x120));
      if (puVar7 != (undefined *)0x0) {
        if (*(char *)(param_1 + 0x9c) == '\0') {
          *(undefined8 *)(puVar7 + 0x50) = 0;
          *(undefined8 *)(puVar7 + 0x58) = 0;
          *(undefined8 *)(puVar7 + 0x60) = 0;
          *(undefined8 *)(puVar7 + 0x68) = 0;
          *(undefined8 *)(puVar7 + 0x70) = 0;
          *(undefined8 *)(puVar7 + 0x78) = 0;
          *(undefined8 *)(puVar7 + 0x80) = 0;
          *(undefined8 *)(puVar7 + 0x88) = 0;
        }
        else {
          *(undefined8 *)(puVar7 + 0xd0) = 0;
          *(undefined8 *)(puVar7 + 0xd8) = 0;
          *(undefined8 *)(puVar7 + 0xe0) = 0;
          *(undefined8 *)(puVar7 + 0xe8) = 0;
          *(undefined8 *)(puVar7 + 0xf0) = 0;
          *(undefined8 *)(puVar7 + 0xf8) = 0;
          *(undefined8 *)(puVar7 + 0x100) = 0;
          *(undefined8 *)(puVar7 + 0x108) = 0;
        }
      }
    }
  }
  else if (param_2 == 3) {
    uVar4 = KeQueryPerformanceCounter((ulonglong *)(param_1 + 0xf8));
    uVar1 = *(ulonglong *)(param_1 + 0xf8);
    uVar5 = (uVar4 >> 0x20) * 10000000;
    *(undefined8 *)(param_1 + 400) = 0;
    *(undefined8 *)(param_1 + 0x198) = 0;
    *(undefined4 *)(param_1 + 0x1b0) = 0;
    *(undefined8 *)(param_1 + 0x110) = 0;
    lVar6 = ((uVar4 & 0xffffffff) * 10000000 + (uVar5 % uVar1 << 0x20)) / uVar1 +
            (uVar5 / uVar1 << 0x20);
    *(undefined8 *)(param_1 + 0x1a8) = 0;
    *(longlong *)(param_1 + 0x108) = lVar6;
    *(longlong *)(param_1 + 0x178) = lVar6;
    *(ulonglong *)(param_1 + 0x1a0) = (ulonglong)*(uint *)(param_1 + 0x70);
    *(undefined8 *)(param_1 + 0x180) = 0;
    *(undefined8 *)(param_1 + 0x188) = 0;
    *(undefined4 *)(param_1 + 0x1c8) = 0;
    bVar2 = FUN_140004cf4(*(longlong *)(param_1 + 0x90),*(uint *)(param_1 + 0x98));
    if ((int)CONCAT71(extraout_var,bVar2) == 0) {
      lVar6 = FUN_1400065b8(param_1 + -8);
      *(longlong *)(param_1 + 0x58) = lVar6;
    }
  }
  *(int *)(param_1 + 0xb4) = param_2;
  return 0;
}



// === FUN_140005b50 @ 140005b50 (size=364) ===

int FUN_140005b50(longlong param_1,uint param_2,uint param_3,uint param_4)

{
  longlong lVar1;
  undefined4 uVar2;
  undefined1 uVar3;
  int iVar4;
  undefined *puVar5;
  undefined8 uVar6;
  int iVar7;
  uint uVar8;
  uint uVar9;
  
  if (*(int *)(param_1 + 0xa0) == 0) {
    iVar4 = -0x3fffff45;
  }
  else {
    uVar2 = *(undefined4 *)(param_1 + 0xdc);
    if (*(char *)(param_1 + 0x14c) == '\0') {
      puVar5 = FUN_140004080(*(int *)(*(longlong *)(param_1 + 0x80) + 0x120));
      if (*(char *)(param_1 + 0x8c) == '\0') {
        *(int *)(puVar5 + 0x16c) = *(int *)(puVar5 + 0x16c) + 1;
      }
      else {
        *(int *)(puVar5 + 0x194) = *(int *)(puVar5 + 0x194) + 1;
      }
      lVar1 = param_1 + 0x150;
      KeAcquireSpinLockRaiseToDpc(lVar1);
      uVar6 = *(undefined8 *)(param_1 + 0xe0);
      KeReleaseSpinLock(lVar1);
      iVar4 = (int)uVar6;
      iVar7 = iVar4 + 1;
      if (*(int *)(param_1 + 0xa4) != 3) {
        iVar7 = iVar4;
      }
      if ((int)(param_2 - iVar7) < 0) {
        iVar4 = -0x3fffffc3;
      }
      else if ((int)(param_2 - iVar7) < 1) {
        uVar9 = *(uint *)(param_1 + 0x90) / *(uint *)(param_1 + 0xa0);
        uVar8 = (param_2 % *(uint *)(param_1 + 0xa0)) * uVar9;
        if ((param_3 >> 9 & 1) == 0) {
          uVar3 = KeAcquireSpinLockRaiseToDpc(lVar1);
          uVar6 = FUN_140004764(param_1 + -0x18,uVar8);
          iVar4 = (int)uVar6;
          KeReleaseSpinLock(lVar1,uVar3);
        }
        else {
          if (uVar9 < param_4) {
            return -0x3ffffff3;
          }
          *(uint *)(param_1 + 0xdc) = param_2;
          iVar4 = (*(code *)PTR__guard_dispatch_icall_140008188)(param_1 + 0x10,uVar8 + param_4);
        }
        if (iVar4 < 0) {
          *(undefined4 *)(param_1 + 0xdc) = uVar2;
        }
      }
      else {
        iVar4 = -0x3fffffc4;
      }
    }
    else {
      iVar4 = -0x3ffffe7c;
    }
  }
  return iVar4;
}



// === FUN_140005cc0 @ 140005cc0 (size=1630) ===

/* WARNING: Function: __security_check_cookie replaced with injection: security_check_cookie */

void FUN_140005cc0(void)

{
  longlong lVar1;
  undefined4 uVar2;
  longlong lVar3;
  undefined8 *puVar4;
  int iVar5;
  undefined1 uVar6;
  int iVar7;
  longlong lVar8;
  undefined *puVar9;
  undefined8 uVar10;
  int *piVar11;
  int iVar12;
  longlong lVar13;
  ulonglong uVar14;
  int *piVar15;
  int iVar16;
  int iVar17;
  longlong lVar18;
  longlong lVar19;
  int iVar20;
  int aiStackY_2a8 [106];
  undefined8 uStackY_100;
  int local_b8;
  longlong local_a8;
  
  iVar17 = 0;
  local_b8 = 0;
  uStackY_100 = 0x140005d04;
  lVar8 = KeQueryPerformanceCounter();
  uStackY_100 = 0x140005d13;
  puVar9 = FUN_140004080(0);
  if (*(longlong *)(puVar9 + 0x298) == 0) {
    lVar13 = SUB168(SEXT816(-0x5c28f5c28f5c28f5) * SEXT816(local_a8),8) + local_a8;
    *(longlong *)(puVar9 + 0x298) = lVar8;
    *(undefined8 *)(puVar9 + 0x2a0) = 1;
    *(longlong *)(puVar9 + 0x2a8) = ((lVar13 >> 6) - (lVar13 >> 0x3f)) + lVar8;
  }
  lVar18 = (longlong)DAT_140012f88;
  iVar20 = 0;
  lVar13 = *(longlong *)(puVar9 + 0x2a8);
  if (0 < lVar18) {
    lVar19 = 0;
    do {
      lVar3 = (&DAT_140012f90)[lVar19];
      if (lVar3 != 0) {
        uStackY_100 = 0x140005db2;
        puVar9 = FUN_140004080(*(int *)(*(longlong *)(lVar3 + 0x98) + 0x120));
        if (puVar9 == (undefined *)0x0) {
          return;
        }
        if (lVar13 < lVar8) {
          iVar20 = iVar20 + 1;
        }
        if (*(int *)(lVar3 + 0xb8) == 0) {
          uStackY_100 = 0x140005ddf;
          uVar6 = KeAcquireSpinLockRaiseToDpc(lVar3 + 0x168);
          *(longlong *)(lVar3 + 0x188) = lVar8;
          uStackY_100 = 0x140005df1;
          KeReleaseSpinLock(lVar3 + 0x168,uVar6);
          if (0 < iVar20) {
            uStackY_100 = 0x140005e05;
            uVar10 = FUN_140006778(lVar3,(longlong)puVar9);
            local_b8 = iVar17 + (int)uVar10;
            iVar17 = local_b8;
          }
        }
        else {
          if (*(char *)(lVar3 + 0xa4) == '\0') {
            *(int *)(puVar9 + 0x168) = *(int *)(puVar9 + 0x168) + 1;
          }
          else {
            *(int *)(puVar9 + 400) = *(int *)(puVar9 + 400) + 1;
          }
          if (*(longlong *)(lVar3 + 0x1b0) < lVar8) {
            lVar1 = lVar3 + 0x168;
            uStackY_100 = 0x140005e46;
            uVar6 = KeAcquireSpinLockRaiseToDpc(lVar1);
            *(undefined8 *)(lVar3 + 0x1c8) = *(undefined8 *)(lVar3 + 0x1b0);
            *(longlong *)(lVar3 + 0x188) = lVar8;
            uStackY_100 = 0x140005e6e;
            uVar10 = FUN_1400068ac(lVar3,lVar8,local_a8);
            if (*(char *)(lVar3 + 0xa4) == '\0') {
              *(int *)(puVar9 + 0x164) = *(int *)(puVar9 + 0x164) + 1;
            }
            else {
              *(int *)(puVar9 + 0x18c) = *(int *)(puVar9 + 0x18c) + 1;
            }
            if ((int)uVar10 == 0) {
              uStackY_100 = 0x140005e95;
              KeReleaseSpinLock(lVar1,uVar6);
            }
            else {
              if (*(char *)(lVar3 + 0x164) == '\0') {
                *(longlong *)(lVar3 + 0xf8) = *(longlong *)(lVar3 + 0xf8) + 1;
              }
              uVar10 = *(undefined8 *)(lVar3 + 0xe8);
              uVar2 = *(undefined4 *)(lVar3 + 0xd8);
              uStackY_100 = 0x140005ed0;
              KeReleaseSpinLock(lVar1,uVar6);
              if ((*(int *)(lVar3 + 0xbc) == 3) &&
                 (lVar1 = *(longlong *)(*(longlong *)(lVar3 + 0x98) + 0x128), lVar1 != 0)) {
                for (puVar4 = *(undefined8 **)(lVar3 + 0x50); puVar4 != (undefined8 *)(lVar3 + 0x50)
                    ; puVar4 = (undefined8 *)*puVar4) {
                  uStackY_100 = 0x140005f2c;
                  (*(code *)PTR__guard_dispatch_icall_140008188)(lVar1,1,uVar10,uVar2);
                  uStackY_100 = 0x140005f3b;
                  KeSetEvent(puVar4[2],0,0);
                }
              }
            }
            iVar17 = local_b8;
            if ((*(char *)(lVar3 + 0x165) != '\0') && (*(longlong *)(lVar3 + 0x60) != 0)) {
              uStackY_100 = 0x140005f6a;
              FUN_14000669c(lVar3);
              *(undefined8 *)(lVar3 + 0x60) = 0;
            }
          }
        }
      }
      lVar19 = lVar19 + 1;
    } while (lVar19 < lVar18);
    piVar15 = (int *)0x0;
    if (0 < iVar20) {
      uStackY_100 = 0x140005fa5;
      puVar9 = FUN_140004080(0);
      lVar8 = *(longlong *)(puVar9 + 0x298);
      lVar13 = *(longlong *)(puVar9 + 0x2a0) + 1;
      if (100 < lVar13) {
        lVar8 = lVar8 + local_a8;
      }
      lVar18 = 1;
      if (lVar13 < 0x65) {
        lVar18 = lVar13;
      }
      *(longlong *)(puVar9 + 0x2a0) = lVar18;
      *(longlong *)(puVar9 + 0x298) = lVar8;
      lVar13 = SUB168(SEXT816(-0x5c28f5c28f5c28f5) * SEXT816(lVar18 * local_a8),8) +
               lVar18 * local_a8;
      *(longlong *)(puVar9 + 0x2a8) = ((lVar13 >> 6) - (lVar13 >> 0x3f)) + lVar8;
      uStackY_100 = 0x14000601b;
      puVar9 = FUN_140004080(0);
      if (puVar9 != (undefined *)0x0) {
        *(undefined4 *)(puVar9 + 0x288) = 0;
        if (*(int *)(puVar9 + 0x28c) < 1) {
          if (*(int *)(puVar9 + 0x290) < 2) {
            *(int *)(puVar9 + 0x290) = *(int *)(puVar9 + 0x290) + 1;
          }
        }
        else {
          *(int *)(puVar9 + 0x194) = *(int *)(puVar9 + 0x194) + 1;
          if ((DAT_140012f80 == 0) && (piVar11 = *(int **)(puVar9 + 0x1a0), piVar11 != (int *)0x0))
          {
            if (*(int *)(puVar9 + 0x290) == 2) {
              uStackY_100 = 0x140006071;
              FUN_1400010ec((longlong)piVar11);
            }
            else {
              iVar20 = piVar11[1];
              iVar16 = iVar20 / 100;
              uStackY_100 = 0x14000608e;
              iVar12 = FUN_14000112c((longlong)piVar11);
              if (iVar16 <= iVar12) {
                iVar16 = iVar16 * 0x40;
                uStackY_100 = 0x1400060bc;
                FUN_1400011d4(piVar11,*(longlong **)(puVar9 + 0x280),iVar16,iVar20,0x10,0x78c,1);
                uStackY_100 = 0x1400060de;
                FUN_140004f2c((longlong)puVar9,*(longlong *)(puVar9 + 0x280),iVar16,0x10,0x78c,1);
                *(undefined4 *)(puVar9 + 0x288) = 1;
              }
            }
          }
          *(undefined8 *)(puVar9 + 0x28c) = 0;
        }
      }
    }
    if (iVar17 != 0) {
      uStackY_100 = 0x140006119;
      puVar9 = FUN_140004080(0);
      if ((puVar9 != (undefined *)0x0) && (0 < *(int *)(puVar9 + 0x278))) {
        *(int *)(puVar9 + 0x16c) = *(int *)(puVar9 + 0x16c) + 1;
        if (DAT_140012f80 == 0) {
          piVar15 = *(int **)(puVar9 + 0x1a0);
        }
        iVar17 = *(int *)(*(longlong *)(puVar9 + 0x1a0) + 4);
        iVar20 = iVar17 / 100;
        iVar16 = iVar20 * 0x40;
        if ((*(uint *)(puVar9 + 0x174) & 2) != 0) {
          lVar8 = 0x10;
          piVar11 = (int *)(puVar9 + 0x230);
          do {
            iVar12 = *(short *)((longlong)piVar11 + -0x7e) + 0x60;
            if (0x5f < iVar12) {
              iVar12 = 0x5f;
            }
            iVar7 = 0;
            if (piVar11[-0x10] != 1) {
              iVar7 = 0;
              if (-1 < iVar12) {
                iVar7 = iVar12;
              }
              iVar7 = *(int *)(&DAT_140012a60 + (longlong)iVar7 * 4);
            }
            *(int *)(((longlong)aiStackY_2a8 - (longlong)puVar9) + (longlong)piVar11) = iVar7;
            iVar12 = *piVar11;
            iVar5 = iVar12 - iVar7;
            if (iVar12 < iVar7) {
              iVar12 = iVar12 + 1;
              *piVar11 = iVar12;
              iVar5 = iVar12 - iVar7;
            }
            if (iVar12 != iVar7 && SBORROW4(iVar12,iVar7) == iVar5 < 0) {
              *piVar11 = iVar12 + -1;
            }
            piVar11 = piVar11 + 1;
            lVar8 = lVar8 + -1;
          } while (lVar8 != 0);
          uStackY_100 = 0x140006221;
          FUN_1400051a8(*(byte **)(puVar9 + 0x270),iVar16,0x10,0x78c,(int *)(puVar9 + 0x230));
        }
        if (1 < *(int *)(puVar9 + 0x278)) {
          piVar11 = *(int **)(puVar9 + 0x270);
          if (0 < iVar20 * 0x10) {
            uVar14 = (ulonglong)(uint)(iVar20 * 0x10);
            do {
              if (*piVar11 < 0x40000) {
                if (*piVar11 < -0x40000) {
                  *piVar11 = -0x40000;
                }
              }
              else {
                *piVar11 = 0x3ffff;
              }
              piVar11 = piVar11 + 1;
              uVar14 = uVar14 - 1;
            } while (uVar14 != 0);
          }
        }
        if (*(int **)(puVar9 + 0x1a8) != (int *)0x0) {
          uStackY_100 = 0x140006298;
          FUN_1400022b0(*(int **)(puVar9 + 0x1a8),*(undefined8 **)(puVar9 + 0x270),iVar16,iVar17,
                        0x10,0x78c,1);
        }
        uStackY_100 = 0x1400062ba;
        FUN_140004f2c((longlong)puVar9,*(longlong *)(puVar9 + 0x270),iVar16,0x10,0x78c,0);
        if (piVar15 != (int *)0x0) {
          uStackY_100 = 0x1400062e6;
          FUN_1400022b0(piVar15,*(undefined8 **)(puVar9 + 0x270),iVar16,iVar17,0x10,0x78c,0);
        }
        *(undefined4 *)(puVar9 + 0x278) = 0;
      }
    }
  }
  return;
}



// === FUN_140006320 @ 140006320 (size=662) ===

void FUN_140006320(longlong param_1,ulonglong param_2,undefined4 param_3)

{
  longlong lVar1;
  undefined *puVar2;
  ulonglong uVar3;
  longlong lVar4;
  uint uVar5;
  ulonglong uVar6;
  int iVar7;
  uint uVar8;
  uint uVar9;
  ulonglong uVar10;
  
  puVar2 = FUN_140004080(*(int *)(*(longlong *)(param_1 + 0x98) + 0x120));
  uVar6 = *(ulonglong *)(param_1 + 0x100);
  uVar3 = (param_2 >> 0x20) * 10000000;
  lVar1 = (uVar3 / uVar6 << 0x20) +
          ((param_2 & 0xffffffff) * 10000000 + (uVar3 % uVar6 << 0x20)) / uVar6;
  lVar4 = (lVar1 - *(longlong *)(param_1 + 0x180)) * (ulonglong)*(uint *)(param_1 + 0x8c);
  lVar4 = SUB168(SEXT816(-0x29406b2a1a85bd43) * SEXT816(lVar4),8) + lVar4;
  uVar6 = (lVar4 >> 0x17) - (lVar4 >> 0x3f);
  uVar5 = (uint)uVar6;
  iVar7 = uVar5 - *(int *)(param_1 + 0x198);
  if (7 < iVar7) {
    uVar8 = iVar7 * *(int *)(param_1 + 0x90);
    uVar3 = (ulonglong)uVar8;
    *(ulonglong *)(param_1 + 0x198) = uVar6 & 0xffffffff;
    if (*(uint *)(param_1 + 0x8c) << 7 <= uVar5) {
      *(undefined8 *)(param_1 + 0x198) = 0;
      *(longlong *)(param_1 + 0x180) = lVar1;
    }
    uVar5 = *(uint *)(param_1 + 0xa8);
    if (uVar5 >> 1 < uVar8) {
      if (*(char *)(param_1 + 0xa4) == '\0') {
        *(int *)(puVar2 + 0x158) = *(int *)(puVar2 + 0x158) + 1;
        if (*(uint *)(param_1 + 0xa8) < uVar8) {
          *(int *)(puVar2 + 0x15c) = *(int *)(puVar2 + 0x15c) + 1;
        }
      }
      else {
        *(int *)(puVar2 + 0x180) = *(int *)(puVar2 + 0x180) + 1;
        if (*(uint *)(param_1 + 0xa8) < uVar8) {
          *(int *)(puVar2 + 0x184) = *(int *)(puVar2 + 0x184) + 1;
        }
      }
    }
    else {
      uVar6 = uVar3;
      if (*(char *)(param_1 + 0xa4) == '\0') {
        if (*(char *)(param_1 + 0x164) != '\0') {
          uVar9 = *(uint *)(param_1 + 0x7c);
          uVar10 = *(ulonglong *)(param_1 + 0xd8);
          if (uVar9 < uVar10) {
            uVar3 = (uVar3 + uVar10) % (ulonglong)uVar5;
            if ((uVar3 < uVar10) && (uVar9 < uVar3)) {
              uVar6 = (ulonglong)(uVar8 + (uVar9 - (uVar8 + (int)uVar10) % uVar5));
            }
          }
          else {
            uVar9 = uVar9 - (int)uVar10;
            uVar6 = (ulonglong)uVar9;
            if (uVar8 < uVar9) {
              uVar6 = uVar3;
            }
          }
        }
        FUN_140005634(param_1,(uint)uVar6,(uint)*(undefined8 *)(param_1 + 0xd0));
        uVar10 = (ulonglong)*(uint *)(param_1 + 0xa8);
        *(longlong *)(param_1 + 0xd8) = *(longlong *)(param_1 + 0xd0);
        uVar3 = *(longlong *)(param_1 + 0xd0) + uVar6;
        *(ulonglong *)(param_1 + 0xd0) = uVar3;
        if (uVar10 <= uVar3) {
          *(ulonglong *)(param_1 + 0xd0) = uVar3 - uVar10;
        }
        if (((*(char *)(param_1 + 0x164) != '\0') && (*(char *)(param_1 + 0x165) == '\0')) &&
           (uVar3 % uVar10 == (ulonglong)*(uint *)(param_1 + 0x7c))) {
          *(undefined1 *)(param_1 + 0x165) = 1;
          (*(code *)PTR__guard_dispatch_icall_140008188)
                    (*(undefined8 *)(*(longlong *)(param_1 + 0x98) + 0x128),8,
                     *(longlong *)(param_1 + 0xe0) + uVar6,(ulonglong)*(uint *)(param_1 + 0x7c),0,0)
          ;
        }
      }
      else {
        FUN_140006adc(param_1,uVar8,(uint)*(undefined8 *)(param_1 + 0xd0));
        uVar3 = uVar3 + *(longlong *)(param_1 + 0xd0);
        *(longlong *)(param_1 + 0xd8) = *(longlong *)(param_1 + 0xd0);
        *(ulonglong *)(param_1 + 0xd0) = uVar3;
        if (*(uint *)(param_1 + 0xa8) <= uVar3) {
          *(ulonglong *)(param_1 + 0xd0) = uVar3 - *(uint *)(param_1 + 0xa8);
        }
      }
      *(longlong *)(param_1 + 0xe8) = *(longlong *)(param_1 + 0xe8) + uVar6;
      *(longlong *)(param_1 + 0xe0) = *(longlong *)(param_1 + 0xe0) + uVar6;
      *(int *)(param_1 + 0x1b8) = *(int *)(param_1 + 0x1b8) + (int)uVar6;
    }
  }
  *(undefined4 *)(param_1 + 0x1d0) = param_3;
  return;
}



// === FUN_1400065b8 @ 1400065b8 (size=226) ===

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

longlong FUN_1400065b8(undefined8 param_1)

{
  int iVar1;
  longlong *plVar2;
  uint *puVar3;
  int iVar4;
  
  if (DAT_140012f84 == 0) {
    DAT_140012f90 = 0;
    DAT_140012f98 = 0;
    DAT_140012fd0 = 0;
    _DAT_140012fa0 = 0;
    uRam0000000140012fa8 = 0;
    _DAT_140012fb0 = 0;
    uRam0000000140012fb8 = 0;
    _DAT_140012fc0 = 0;
    DAT_140012fc8 = 0;
    DAT_140012fd8 = ExAllocateTimer(FUN_140005cc0,0,4);
    if (DAT_140012fd8 != 0) {
      ExSetTimer(DAT_140012fd8,0xffffffffffffd8f0,10000,0);
    }
  }
  iVar1 = DAT_140012f88;
  iVar4 = 0;
  plVar2 = &DAT_140012f90;
  do {
    if (*plVar2 == 0) {
      (&DAT_140012f90)[iVar4] = param_1;
      if (iVar1 <= iVar4) {
        DAT_140012f88 = iVar4 + 1;
      }
      DAT_140012f84 = DAT_140012f84 + 1;
      puVar3 = (uint *)FUN_140004080(0);
      *puVar3 = DAT_140012f84 << 0x10 | *puVar3 & 0xffff;
      return DAT_140012fd8;
    }
    iVar4 = iVar4 + 1;
    plVar2 = plVar2 + 1;
  } while ((longlong)plVar2 < 0x140012fd8);
  return 0;
}



// === FUN_14000669c @ 14000669c (size=218) ===

void FUN_14000669c(longlong param_1)

{
  uint uVar1;
  longlong *plVar2;
  uint *puVar3;
  undefined *puVar4;
  longlong *plVar5;
  int iVar6;
  
  iVar6 = 0;
  plVar2 = &DAT_140012f90;
  plVar5 = &DAT_140012fd8;
  do {
    if (*plVar2 == param_1) {
      (&DAT_140012f90)[iVar6] = 0;
      if (0 < DAT_140012f84) {
        DAT_140012f84 = DAT_140012f84 + -1;
      }
      iVar6 = 9;
      goto LAB_1400066e6;
    }
    iVar6 = iVar6 + 1;
    plVar2 = plVar2 + 1;
  } while ((longlong)plVar2 < 0x140012fd8);
  goto LAB_140006704;
  while (iVar6 = iVar6 + -1, DAT_140012f88 = 0, 0x140012f90 < (longlong)plVar5) {
LAB_1400066e6:
    plVar5 = plVar5 + -1;
    DAT_140012f88 = iVar6;
    if (*plVar5 != 0) break;
  }
LAB_140006704:
  puVar3 = (uint *)FUN_140004080(0);
  iVar6 = DAT_140012f84;
  uVar1 = *puVar3;
  *puVar3 = DAT_140012f84 << 0x10 | uVar1 & 0xffff;
  if ((iVar6 == 0) && (DAT_140012fd8 != 0)) {
    ExDeleteTimer(DAT_140012fd8,1,CONCAT71((uint7)(uint3)((uVar1 & 0xffff) >> 8),1),0);
    DAT_140012fd8 = 0;
    puVar4 = FUN_140004080(0);
    if (puVar4 != (undefined *)0x0) {
      *(undefined8 *)(puVar4 + 0x298) = 0;
      *(undefined8 *)(puVar4 + 0x2a0) = 0;
      *(undefined8 *)(puVar4 + 0x2a8) = 0;
    }
  }
  return;
}



// === FUN_140006778 @ 140006778 (size=308) ===

undefined8 FUN_140006778(longlong param_1,longlong param_2)

{
  int iVar1;
  int *piVar2;
  int iVar3;
  undefined8 uVar4;
  
  uVar4 = 0;
  if (*(char *)(param_1 + 0xa4) == '\0') {
    piVar2 = *(int **)(param_1 + 0x170);
    iVar1 = *(int *)(*(longlong *)(param_2 + 0x1a0) + 4);
    if (piVar2 != (int *)0x0) {
      iVar3 = FUN_14000112c((longlong)piVar2);
      if (iVar1 / 100 <= iVar3) {
        piVar2[1] = iVar1;
        FUN_1400011d4(piVar2,*(longlong **)(param_2 + 0x270),(iVar1 / 100) * 0x40,iVar1,0x10,0x78c,
                      (-(uint)(*(int *)(param_2 + 0x278) != 0) & 0xfffff) + 1);
        *(int *)(param_2 + 0x278) = *(int *)(param_2 + 0x278) + 1;
        uVar4 = 1;
      }
    }
  }
  else {
    if (*(int *)(param_2 + 0x288) == 1) {
      piVar2 = *(int **)(param_1 + 0x170);
      iVar1 = *(int *)(*(longlong *)(param_2 + 0x1a0) + 4);
      if (piVar2 != (int *)0x0) {
        piVar2[1] = iVar1;
        FUN_1400022b0(piVar2,*(undefined8 **)(param_2 + 0x280),(iVar1 / 100) * 0x40,iVar1,0x10,0x78c
                      ,1);
      }
    }
    *(int *)(param_2 + 0x28c) = *(int *)(param_2 + 0x28c) + 1;
  }
  return uVar4;
}



// === FUN_1400068ac @ 1400068ac (size=558) ===

undefined8 FUN_1400068ac(longlong param_1,longlong param_2,longlong param_3)

{
  uint uVar1;
  ulonglong uVar2;
  undefined8 uVar3;
  uint uVar4;
  longlong lVar5;
  ulonglong uVar6;
  ulonglong uVar7;
  longlong lVar8;
  
  lVar8 = (longlong)*(int *)(param_1 + 0x1c0);
  uVar1 = *(uint *)(param_1 + 0x1bc);
  uVar7 = (ulonglong)uVar1;
  if (*(longlong *)(param_1 + 400) == 0) {
    *(undefined8 *)(param_1 + 0x1a0) = 0;
    *(longlong *)(param_1 + 400) = param_2;
    *(longlong *)(param_1 + 0x1b0) =
         (lVar8 * param_3) / (longlong)(ulonglong)*(uint *)(param_1 + 0x8c) + param_2;
    uVar3 = 0;
  }
  else {
    if (*(char *)(param_1 + 0xa4) == '\0') {
      if (*(char *)(param_1 + 0x164) != '\0') {
        uVar4 = *(uint *)(param_1 + 0x7c);
        uVar2 = *(ulonglong *)(param_1 + 0xd8);
        if (uVar4 < uVar2) {
          uVar6 = (uVar2 + uVar7) % (ulonglong)*(uint *)(param_1 + 0xa8);
          if ((uVar6 < uVar2) && (uVar4 < uVar6)) {
            uVar7 = (ulonglong)(uVar1 + (uVar4 - ((int)uVar2 + uVar1) % *(uint *)(param_1 + 0xa8)));
          }
        }
        else {
          uVar4 = uVar4 - (int)uVar2;
          if (uVar1 < uVar4) {
            uVar4 = uVar1;
          }
          uVar7 = (ulonglong)uVar4;
        }
      }
      FUN_140005634(param_1,(uint)uVar7,(uint)*(undefined8 *)(param_1 + 0xd0));
      uVar6 = (ulonglong)*(uint *)(param_1 + 0xa8);
      *(longlong *)(param_1 + 0xd8) = *(longlong *)(param_1 + 0xd0);
      uVar2 = *(longlong *)(param_1 + 0xd0) + uVar7;
      *(ulonglong *)(param_1 + 0xd0) = uVar2;
      if (uVar6 <= uVar2) {
        *(ulonglong *)(param_1 + 0xd0) = uVar2 - uVar6;
      }
      if (((*(char *)(param_1 + 0x164) != '\0') && (*(char *)(param_1 + 0x165) == '\0')) &&
         (uVar2 % uVar6 == (ulonglong)*(uint *)(param_1 + 0x7c))) {
        *(undefined1 *)(param_1 + 0x165) = 1;
        (*(code *)PTR__guard_dispatch_icall_140008188)
                  (*(undefined8 *)(*(longlong *)(param_1 + 0x98) + 0x128),8,
                   *(longlong *)(param_1 + 0xe0) + uVar7,(ulonglong)*(uint *)(param_1 + 0x7c),0,0);
      }
    }
    else {
      FUN_140006adc(param_1,uVar1,(uint)*(undefined8 *)(param_1 + 0xd0));
      *(longlong *)(param_1 + 0xd8) = *(longlong *)(param_1 + 0xd0);
      uVar2 = *(longlong *)(param_1 + 0xd0) + uVar7;
      *(ulonglong *)(param_1 + 0xd0) = uVar2;
      if (*(uint *)(param_1 + 0xa8) <= uVar2) {
        *(ulonglong *)(param_1 + 0xd0) = uVar2 - *(uint *)(param_1 + 0xa8);
      }
    }
    lVar5 = *(longlong *)(param_1 + 0x1a0) + lVar8;
    *(longlong *)(param_1 + 0xe8) = *(longlong *)(param_1 + 0xe8) + uVar7;
    *(longlong *)(param_1 + 0xe0) = *(longlong *)(param_1 + 0xe0) + uVar7;
    *(int *)(param_1 + 0x1b8) = (int)uVar7;
    *(longlong *)(param_1 + 0x1a0) = lVar5;
    if ((longlong)(ulonglong)(*(uint *)(param_1 + 0x8c) << 7) <= lVar5) {
      *(undefined8 *)(param_1 + 0x1a0) = 0;
      lVar5 = 0;
      *(longlong *)(param_1 + 400) = param_2;
    }
    *(longlong *)(param_1 + 0x1b0) =
         ((lVar8 + lVar5) * param_3) / (longlong)(ulonglong)*(uint *)(param_1 + 0x8c) +
         *(longlong *)(param_1 + 400);
    uVar3 = 1;
  }
  return uVar3;
}



// === FUN_140006adc @ 140006adc (size=801) ===

void FUN_140006adc(longlong param_1,uint param_2,uint param_3)

{
  bool bVar1;
  undefined7 extraout_var;
  int *piVar2;
  ulonglong uVar3;
  undefined *puVar4;
  int iVar5;
  uint uVar6;
  
  iVar5 = 0;
  if (*(longlong *)(param_1 + 0x178) != 0) {
    bVar1 = FUN_140004cf4(*(longlong *)(param_1 + 0x98),*(uint *)(param_1 + 0xa0));
    if ((int)CONCAT71(extraout_var,bVar1) == 0) {
      if (*(int *)(param_1 + 0xb8) == 0) {
        if (*(longlong *)(param_1 + 0x170) != 0) {
          uVar3 = FUN_140001144(*(longlong *)(param_1 + 0x170),*(int *)(param_1 + 0x8c));
          if ((uint)(*(ushort *)(param_1 + 0x86) >> 3) * (uint)*(ushort *)(param_1 + 0x88) *
              (int)uVar3 < param_2) {
            iVar5 = 100;
            FUN_140007940(*(longlong **)(param_1 + 0x178),0,(undefined1 *)(ulonglong)param_2);
          }
          else {
            uVar3 = FUN_1400011d4(*(int **)(param_1 + 0x170),*(longlong **)(param_1 + 0x178),param_2
                                  ,*(int *)(param_1 + 0x8c),(uint)*(ushort *)(param_1 + 0x88),
                                  (uint)*(ushort *)(param_1 + 0x86),1);
            iVar5 = (int)uVar3;
          }
          puVar4 = FUN_140004080(*(int *)(*(longlong *)(param_1 + 0x98) + 0x120));
          if (puVar4 != (undefined *)0x0) {
            *(int *)(puVar4 + 0x194) = *(int *)(puVar4 + 0x194) + 1;
            if (iVar5 != 0) {
              *(int *)(puVar4 + 0x28) = *(int *)(puVar4 + 0x28) + 1;
            }
            uVar6 = (uint)(*(ushort *)(param_1 + 0x86) >> 3) * (uint)*(ushort *)(param_1 + 0x88);
            if (uVar6 != 0) {
              uVar6 = param_2 / uVar6;
              if (uVar6 < 0x400) {
                if (uVar6 < 0x200) {
                  if (uVar6 < 0x100) {
                    *(int *)(puVar4 + 0x3c) = *(int *)(puVar4 + 0x3c) + 1;
                  }
                  else {
                    *(int *)(puVar4 + 0x40) = *(int *)(puVar4 + 0x40) + 1;
                  }
                }
                else {
                  *(int *)(puVar4 + 0x44) = *(int *)(puVar4 + 0x44) + 1;
                }
              }
              else {
                *(int *)(puVar4 + 0x48) = *(int *)(puVar4 + 0x48) + 1;
              }
              if (*(uint *)(puVar4 + 0x19c) < uVar6 * 3) {
                *(int *)(puVar4 + 0x140) = *(int *)(puVar4 + 0x140) + 1;
              }
            }
          }
        }
      }
      else {
        if (DAT_140012f80 == 0) {
          piVar2 = (int *)FUN_140004068((ulonglong)*(uint *)(*(longlong *)(param_1 + 0x98) + 0x120))
          ;
          if (piVar2 != (int *)0x0) {
            uVar3 = FUN_1400011d4(piVar2,*(longlong **)(param_1 + 0x178),param_2,
                                  *(int *)(param_1 + 0x8c),(uint)*(ushort *)(param_1 + 0x88),
                                  (uint)*(ushort *)(param_1 + 0x86),1);
            iVar5 = (int)uVar3;
          }
        }
        puVar4 = FUN_140004080(*(int *)(*(longlong *)(param_1 + 0x98) + 0x120));
        if (puVar4 != (undefined *)0x0) {
          FUN_140004f2c((longlong)puVar4,*(longlong *)(param_1 + 0x178),param_2,
                        (uint)*(ushort *)(param_1 + 0x88),(uint)*(ushort *)(param_1 + 0x86),1);
          if (iVar5 != 0) {
            *(int *)(puVar4 + 0x28) = *(int *)(puVar4 + 0x28) + 1;
          }
          uVar6 = (uint)(*(ushort *)(param_1 + 0x86) >> 3) * (uint)*(ushort *)(param_1 + 0x88);
          if (uVar6 != 0) {
            uVar6 = param_2 / uVar6;
            if (uVar6 < 0x400) {
              if (uVar6 < 0x200) {
                if (uVar6 < 0x100) {
                  *(int *)(puVar4 + 0x3c) = *(int *)(puVar4 + 0x3c) + 1;
                }
                else {
                  *(int *)(puVar4 + 0x40) = *(int *)(puVar4 + 0x40) + 1;
                }
              }
              else {
                *(int *)(puVar4 + 0x44) = *(int *)(puVar4 + 0x44) + 1;
              }
            }
            else {
              *(int *)(puVar4 + 0x48) = *(int *)(puVar4 + 0x48) + 1;
            }
            if (*(uint *)(puVar4 + 0x19c) < uVar6 * 3) {
              *(int *)(puVar4 + 0x140) = *(int *)(puVar4 + 0x140) + 1;
            }
          }
        }
      }
    }
    else {
      piVar2 = (int *)FUN_140004050((ulonglong)*(uint *)(*(longlong *)(param_1 + 0x98) + 0x120));
      if (piVar2 != (int *)0x0) {
        FUN_1400011d4(piVar2,*(longlong **)(param_1 + 0x178),param_2,*(int *)(param_1 + 0x8c),
                      (uint)*(ushort *)(param_1 + 0x88),(uint)*(ushort *)(param_1 + 0x86),1);
      }
    }
    uVar6 = *(int *)(param_1 + 0xa8) - param_3;
    if (param_2 < uVar6) {
      uVar6 = param_2;
    }
    FUN_140007680((undefined8 *)((ulonglong)param_3 + *(longlong *)(param_1 + 0xb0)),
                  *(undefined8 **)(param_1 + 0x178),(ulonglong)uVar6);
    if (uVar6 < param_2) {
      FUN_140007680(*(undefined8 **)(param_1 + 0xb0),
                    (undefined8 *)(*(longlong *)(param_1 + 0x178) + (ulonglong)uVar6),
                    (ulonglong)(param_2 - uVar6));
    }
  }
  return;
}



// === FUN_140006e00 @ 140006e00 (size=29) ===

undefined8 * FUN_140006e00(undefined8 *param_1,undefined8 *param_2)

{
  *(undefined4 *)(param_1 + 1) = 0;
  *param_1 = &PTR_NonDelegatingQueryInterface_140009228;
  if (param_2 == (undefined8 *)0x0) {
    param_2 = param_1;
  }
  param_1[2] = param_2;
  return param_1;
}



// === FUN_140006e20 @ 140006e20 (size=11) ===

void FUN_140006e20(undefined8 *param_1)

{
  *param_1 = &PTR_NonDelegatingQueryInterface_140009228;
  return;
}



// === FUN_140006e30 @ 140006e30 (size=45) ===

undefined8 * FUN_140006e30(undefined8 *param_1,byte param_2)

{
  *param_1 = &PTR_NonDelegatingQueryInterface_140009228;
  if ((param_2 & 1) != 0) {
    ExFreePool();
  }
  return param_1;
}



// === FUN_140006e60 @ 140006e60 (size=8) ===

undefined4 FUN_140006e60(longlong param_1)

{
  LOCK();
  *(int *)(param_1 + 8) = *(int *)(param_1 + 8) + 1;
  UNLOCK();
  return *(undefined4 *)(param_1 + 8);
}



// === NonDelegatingQueryInterface @ 140006e70 (size=80) ===

/* Library Function - Single Match
    public: virtual long __cdecl CUnknown::NonDelegatingQueryInterface(struct _GUID const &
   __ptr64,void * __ptr64 * __ptr64) __ptr64
   
   Library: Visual Studio 2019 Release */

long __thiscall CUnknown::NonDelegatingQueryInterface(CUnknown *this,_GUID *param_1,void **param_2)

{
  byte bVar1;
  long lVar2;
  longlong *plVar3;
  
  if ((*(longlong *)param_1 == DAT_1400088e0) && (*(longlong *)(param_1 + 8) == DAT_1400088e8)) {
    bVar1 = 1;
  }
  else {
    bVar1 = 0;
  }
  plVar3 = (longlong *)(-(ulonglong)bVar1 & (ulonglong)this);
  *param_2 = plVar3;
  if (plVar3 == (longlong *)0x0) {
    lVar2 = -0x3ffffff3;
  }
  else {
    (**(code **)(*plVar3 + 8))();
    lVar2 = 0;
  }
  return lVar2;
}



// === NonDelegatingRelease @ 140006ed0 (size=46) ===

/* Library Function - Single Match
    public: virtual unsigned long __cdecl CUnknown::NonDelegatingRelease(void) __ptr64
   
   Library: Visual Studio 2019 Release */

ulong __thiscall CUnknown::NonDelegatingRelease(CUnknown *this)

{
  CUnknown *pCVar1;
  int iVar2;
  ulong uVar3;
  
  LOCK();
  pCVar1 = this + 8;
  iVar2 = *(int *)pCVar1;
  *(int *)pCVar1 = *(int *)pCVar1 + -1;
  UNLOCK();
  if (iVar2 == 1) {
    *(int *)(this + 8) = *(int *)(this + 8) + 1;
    (**(code **)(*(longlong *)this + 0x18))(this,1);
    uVar3 = 0;
  }
  else {
    uVar3 = *(ulong *)(this + 8);
  }
  return uVar3;
}



// === __security_check_cookie @ 140006f10 (size=30) ===

/* WARNING: This is an inlined function */

void __cdecl __security_check_cookie(uintptr_t _StackCookie)

{
  if ((_StackCookie == DAT_140012be0) && ((short)(_StackCookie >> 0x30) == 0)) {
    return;
  }
  FUN_140006f30();
  return;
}



// === FUN_140006f30 @ 140006f30 (size=8) ===

void FUN_140006f30(void)

{
  code *pcVar1;
  
  pcVar1 = (code *)swi(0x29);
  (*pcVar1)(2);
  pcVar1 = (code *)swi(3);
  (*pcVar1)();
  return;
}



// === _guard_check_icall @ 140006f40 (size=3) ===

void _guard_check_icall(void)

{
  return;
}



// === __GSHandlerCheck @ 140006f50 (size=29) ===

/* Library Function - Single Match
    __GSHandlerCheck
   
   Library: Visual Studio 2019 Release */

undefined8
__GSHandlerCheck(undefined8 param_1,undefined8 param_2,undefined8 param_3,longlong param_4)

{
  __GSHandlerCheckCommon(param_2,param_4);
  return 1;
}



// === __GSHandlerCheckCommon @ 140006f70 (size=95) ===

/* WARNING: Function: __security_check_cookie replaced with injection: security_check_cookie */
/* Library Function - Single Match
    __GSHandlerCheckCommon
   
   Library: Visual Studio 2019 Release */

ulonglong __GSHandlerCheckCommon(undefined8 param_1,longlong param_2)

{
  byte bVar1;
  ulonglong uVar2;
  
  uVar2 = *(ulonglong *)(param_2 + 8);
  bVar1 = *(byte *)((ulonglong)*(uint *)(*(longlong *)(param_2 + 0x10) + 8) + 3 + uVar2);
  if ((bVar1 & 0xf) != 0) {
    uVar2 = (ulonglong)(bVar1 & 0xfffffff0);
  }
  return uVar2;
}



// === swprintf @ 140006fd0 (size=6) ===

/* WARNING: Unknown calling convention -- yet parameter storage is locked */

int swprintf(wchar_t *_String,size_t _Count,wchar_t *_Format,...)

{
  int iVar1;
  
                    /* WARNING: Could not recover jumptable at 0x000140006fd0. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  iVar1 = swprintf(_String,_Count,_Format);
  return iVar1;
}



// === FUN_140006fe0 @ 140006fe0 (size=155) ===

/* WARNING: Removing unreachable block (ram,0x00014000701d) */
/* WARNING: Removing unreachable block (ram,0x000140006ff9) */
/* WARNING: Removing unreachable block (ram,0x000140006fea) */

undefined8 FUN_140006fe0(void)

{
  int *piVar1;
  longlong lVar2;
  uint uVar3;
  byte bVar4;
  byte in_XCR0;
  
  piVar1 = (int *)cpuid_basic_info(0);
  bVar4 = 0;
  lVar2 = cpuid_Version_info(1);
  uVar3 = *(uint *)(lVar2 + 0xc);
  if (6 < *piVar1) {
    bVar4 = 0;
    lVar2 = cpuid_Extended_Feature_Enumeration_info(7);
    if ((*(uint *)(lVar2 + 4) >> 9 & 1) != 0) {
      bVar4 = 2;
    }
  }
  if (((((uVar3 >> 0x14 & 1) != 0) && ((uVar3 >> 0x1b & 1) != 0)) && ((uVar3 >> 0x1c & 1) != 0)) &&
     ((in_XCR0 & 6) == 6)) {
    bVar4 = bVar4 | 4;
  }
  DAT_140012c00 = bVar4 | 1;
  return 0;
}



// === FUN_140007080 @ 140007080 (size=88) ===

void FUN_140007080(void)

{
  int iVar1;
  undefined8 uVar2;
  undefined *local_28;
  undefined4 local_20;
  undefined8 local_1c;
  undefined4 local_14;
  
  uVar2 = FUN_140007524();
  if ((char)uVar2 != '\0') {
    local_1c = 0;
    local_14 = 0;
    local_28 = &DAT_140009288;
    local_20 = 0x18;
    iVar1 = WdfLdrQueryInterface(&local_28);
    if (-1 < iVar1) {
      (*(code *)PTR__guard_dispatch_icall_140008188)(&DAT_140012c10,DAT_140013210);
    }
  }
  return;
}



// === FxStubDriverUnloadCommon @ 1400070dc (size=47) ===

/* Library Function - Single Match
    void __cdecl FxStubDriverUnloadCommon(void)
   
   Library: Visual Studio 2019 Release */

void __cdecl FxStubDriverUnloadCommon(void)

{
  FUN_140007538(&DAT_140012c10);
  WdfVersionUnbind(&DAT_1400131f8,&DAT_140012c10,DAT_140013210);
  return;
}



// === entry @ 140007110 (size=43) ===

void entry(longlong param_1,undefined8 param_2)

{
  FUN_14001c114();
  FUN_14000713c(param_1,param_2);
  return;
}



// === FUN_14000713c @ 14000713c (size=344) ===

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

int FUN_14000713c(longlong param_1,undefined8 param_2)

{
  int iVar1;
  undefined8 uVar2;
  
  if (param_1 == 0) {
    iVar1 = FUN_14001c000(0,param_2);
  }
  else {
    _DAT_1400131f8 = 0x2080000;
    _DAT_140013200 = &DAT_140012fe0;
    _DAT_140013218 = param_1;
    RtlCopyUnicodeString(&DAT_1400131f8);
    iVar1 = WdfVersionBind(param_1,&DAT_1400131f8,&DAT_140012c10,&DAT_140013210);
    if (-1 < iVar1) {
      DAT_1400131e8 = *(code **)(DAT_140013208 + 0x648);
      iVar1 = FUN_1400073a4(&DAT_140012c10);
      if (-1 < iVar1) {
        uVar2 = FUN_1400072e0();
        iVar1 = (int)uVar2;
        if (-1 < iVar1) {
          iVar1 = FUN_14001c000(param_1,param_2);
          if (-1 < iVar1) {
            if (*(char *)(DAT_140013210 + 0x30) == '\0') {
              if ((*(uint *)(DAT_140013210 + 8) & 2) != 0) {
                DAT_1400131e8 = FUN_1400072a0;
              }
            }
            else {
              if (*(longlong *)(param_1 + 0x68) != 0) {
                DAT_140013220 = *(longlong *)(param_1 + 0x68);
              }
              *(code **)(param_1 + 0x68) = FxStubDriverUnload;
            }
            return 0;
          }
          DbgPrintEx(0x4d,0,"DriverEntry failed 0x%x for driver %wZ\n",iVar1,&DAT_1400131f8);
          FUN_140007080();
        }
      }
      FxStubDriverUnloadCommon();
    }
  }
  return iVar1;
}



// === FUN_1400072a0 @ 1400072a0 (size=14) ===

void FUN_1400072a0(void)

{
  FxStubDriverUnloadCommon();
  return;
}



// === FxStubDriverUnload @ 1400072b0 (size=44) ===

/* Library Function - Single Match
    FxStubDriverUnload
   
   Library: Visual Studio 2019 Release */

void FxStubDriverUnload(void)

{
  if ((DAT_140013220 != (code *)0x0) && (DAT_140013220 != FxStubDriverUnload)) {
    (*(code *)PTR__guard_dispatch_icall_140008188)();
  }
  FxStubDriverUnloadCommon();
  return;
}



// === FUN_1400072e0 @ 1400072e0 (size=193) ===

/* WARNING: Removing unreachable block (ram,0x0001400072fd) */

undefined8 FUN_1400072e0(void)

{
  undefined8 uVar1;
  uint *puVar2;
  
  puVar2 = &DAT_140012c70;
  while( true ) {
    for (; (puVar2 + 2 < (uint *)((longlong)&DAT_140012c70 + 1) && (*(longlong *)puVar2 == 0));
        puVar2 = puVar2 + 2) {
    }
    if ((uint *)0x140012c6f < puVar2) break;
    if (((&DAT_140012c70 < puVar2 + 10) || (*puVar2 != 0x28)) || (puVar2 == (uint *)0x0)) {
      DbgPrintEx(0x4d,0,"FxGetNextObjectContextTypeInfo failed\n");
      return 0xc000007b;
    }
    if (*(longlong *)(puVar2 + 8) != 0) {
      uVar1 = (*(code *)PTR__guard_dispatch_icall_140008188)();
      *(undefined8 *)(puVar2 + 6) = uVar1;
    }
    puVar2 = (uint *)((ulonglong)*puVar2 + (longlong)puVar2);
  }
  return 0;
}



// === FUN_1400073a4 @ 1400073a4 (size=382) ===

/* WARNING: Removing unreachable block (ram,0x0001400073d0) */

int FUN_1400073a4(undefined8 param_1)

{
  int iVar1;
  uint *puVar2;
  
  iVar1 = 0;
  puVar2 = &DAT_140012c50;
  while( true ) {
    for (; (puVar2 + 2 < (uint *)((longlong)&DAT_140012c50 + 1) && (*(longlong *)puVar2 == 0));
        puVar2 = puVar2 + 2) {
    }
    if ((uint *)0x140012c4f < puVar2) break;
    if (((&DAT_140012c50 < puVar2 + 0x14) || (*puVar2 != 0x50)) || (puVar2 == (uint *)0x0)) {
      DbgPrintEx(0x4d,0,"FxGetNextClassBindInfo failed\n");
      return -0x3fffff85;
    }
    PTR_DAT_140012c58 = (undefined *)puVar2;
    if (*(longlong *)(puVar2 + 0xe) == 0) {
      iVar1 = WdfVersionBindClass(param_1,DAT_140013210,puVar2);
      if (iVar1 < 0) {
        DbgPrintEx(0x4d,0,
                   "FxStubBindClasses: VersionBindClass WDF_CLASS_BIND_INFO 0x%p, class %S, returned status 0x%x\n"
                   ,puVar2,*(undefined8 *)(puVar2 + 2),iVar1);
        return iVar1;
      }
    }
    else {
      iVar1 = (*(code *)PTR__guard_dispatch_icall_140008188)
                        (WdfVersionBindClass,param_1,DAT_140013210,puVar2);
      if (iVar1 < 0) {
        DbgPrintEx(0x4d,0,
                   "FxStubBindClasses: ClientBindClass %p, WDF_CLASS_BIND_INFO 0x%p, class %S, returned status 0x%x\n"
                   ,*(undefined8 *)(puVar2 + 0xe),puVar2,*(undefined8 *)(puVar2 + 2),iVar1);
        return iVar1;
      }
    }
    puVar2 = (uint *)((ulonglong)*puVar2 + (longlong)puVar2);
  }
  return iVar1;
}



// === FUN_140007524 @ 140007524 (size=18) ===

undefined8 FUN_140007524(void)

{
  return CONCAT71(0x140012c,PTR_DAT_140012c58 != &DAT_140012c40);
}



// === FUN_140007538 @ 140007538 (size=209) ===

void FUN_140007538(undefined8 param_1)

{
  uint *puVar1;
  uint *puVar2;
  uint *puVar3;
  
  puVar1 = &DAT_140012c50;
  if (PTR_DAT_140012c58 != &DAT_140012c40) {
    puVar3 = (uint *)(PTR_DAT_140012c58 + 0x50);
    while( true ) {
      for (; (puVar1 + 2 <= puVar3 && (*(longlong *)puVar1 == 0)); puVar1 = puVar1 + 2) {
      }
      puVar2 = puVar3;
      if (((puVar1 < puVar3) && ((puVar3 < puVar1 + 0x14 || (puVar2 = puVar1, *puVar1 != 0x50)))) ||
         (puVar2 == (uint *)0x0)) break;
      if (puVar3 <= puVar2) {
        return;
      }
      if (*(longlong *)(puVar2 + 0x10) == 0) {
        WdfVersionUnbindClass(param_1,DAT_140013210,puVar2);
      }
      else {
        (*(code *)PTR__guard_dispatch_icall_140008188)
                  (WdfVersionUnbindClass,param_1,DAT_140013210,puVar2);
      }
      puVar1 = (uint *)((longlong)puVar2 + (ulonglong)*puVar2);
    }
    DbgPrintEx(0x4d,0,"FxGetNextClassBindInfo failed\n");
  }
  return;
}



// === WdfVersionBind @ 14000760a (size=6) ===

void WdfVersionBind(void)

{
                    /* WARNING: Could not recover jumptable at 0x00014000760a. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  WdfVersionBind();
  return;
}



// === WdfLdrQueryInterface @ 140007610 (size=6) ===

void WdfLdrQueryInterface(void)

{
                    /* WARNING: Could not recover jumptable at 0x000140007610. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  WdfLdrQueryInterface();
  return;
}



// === WdfVersionUnbind @ 140007616 (size=6) ===

void WdfVersionUnbind(void)

{
                    /* WARNING: Could not recover jumptable at 0x000140007616. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  WdfVersionUnbind();
  return;
}



// === WdfVersionBindClass @ 140007620 (size=6) ===

void WdfVersionBindClass(void)

{
                    /* WARNING: Could not recover jumptable at 0x000140007620. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  WdfVersionBindClass();
  return;
}



// === WdfVersionUnbindClass @ 140007630 (size=6) ===

void WdfVersionUnbindClass(void)

{
                    /* WARNING: Could not recover jumptable at 0x000140007630. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  WdfVersionUnbindClass();
  return;
}



// === _guard_dispatch_icall @ 140007650 (size=2) ===

/* WARNING: This is an inlined function */

void _guard_dispatch_icall(void)

{
  code *UNRECOVERED_JUMPTABLE;
  
                    /* WARNING: Could not recover jumptable at 0x000140007650. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (*UNRECOVERED_JUMPTABLE)();
  return;
}



// === _guard_dispatch_icall @ 140007670 (size=6) ===

/* WARNING: This is an inlined function */
/* WARNING: Switch with 1 destination removed at 0x000140007670 */

void _guard_dispatch_icall(void)

{
  code *UNRECOVERED_JUMPTABLE;
  
                    /* WARNING: Could not recover jumptable at 0x000140007650. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (*UNRECOVERED_JUMPTABLE)();
  return;
}



// === FUN_140007680 @ 140007680 (size=537) ===

void FUN_140007680(undefined8 *param_1,undefined8 *param_2,ulonglong param_3)

{
  undefined4 *puVar1;
  undefined1 *puVar2;
  undefined8 uVar3;
  undefined8 uVar4;
  undefined8 uVar5;
  undefined8 uVar6;
  undefined8 uVar7;
  undefined8 uVar8;
  undefined1 *puVar9;
  undefined8 *puVar10;
  undefined8 *puVar11;
  undefined4 *puVar12;
  undefined4 *puVar13;
  undefined4 *puVar14;
  longlong lVar15;
  ulonglong uVar16;
  ulonglong uVar17;
  undefined4 uVar18;
  undefined4 uVar19;
  undefined4 uVar20;
  undefined4 uVar21;
  undefined4 uVar22;
  undefined4 uVar23;
  undefined4 uVar24;
  undefined4 uVar25;
  
  if (param_3 < 8) {
    if (param_3 != 0) {
      puVar10 = param_1;
      if (param_2 < param_1) {
        puVar9 = (undefined1 *)((longlong)param_1 + param_3);
        do {
          puVar2 = puVar9 + ((longlong)param_2 - (longlong)param_1) + -1;
          puVar9 = puVar9 + -1;
          param_3 = param_3 - 1;
          *puVar9 = *puVar2;
        } while (param_3 != 0);
        return;
      }
      do {
        param_3 = param_3 - 1;
        *(undefined1 *)puVar10 =
             *(undefined1 *)((longlong)puVar10 + ((longlong)param_2 - (longlong)param_1));
        puVar10 = (undefined8 *)((longlong)puVar10 + 1);
      } while (param_3 != 0);
    }
    return;
  }
  if (param_3 < 0x11) {
    uVar3 = *(undefined8 *)((longlong)param_2 + (param_3 - 8));
    *param_1 = *param_2;
    *(undefined8 *)((longlong)param_1 + (param_3 - 8)) = uVar3;
    return;
  }
  if (param_3 < 0x21) {
    uVar3 = param_2[1];
    puVar10 = (undefined8 *)((longlong)param_2 + (param_3 - 0x10));
    uVar4 = *puVar10;
    uVar5 = puVar10[1];
    *param_1 = *param_2;
    param_1[1] = uVar3;
    puVar10 = (undefined8 *)((longlong)param_1 + (param_3 - 0x10));
    *puVar10 = uVar4;
    puVar10[1] = uVar5;
    return;
  }
  lVar15 = (longlong)param_2 - (longlong)param_1;
  if ((param_2 < param_1) && (param_1 < (undefined8 *)((longlong)param_2 + param_3))) {
    puVar14 = (undefined4 *)((longlong)param_1 + lVar15 + (param_3 - 0x10));
    uVar18 = puVar14[1];
    uVar20 = puVar14[2];
    uVar22 = puVar14[3];
    puVar12 = (undefined4 *)((longlong)param_1 + (param_3 - 0x10));
    uVar16 = param_3 - 0x10;
    puVar13 = puVar12;
    uVar19 = *puVar14;
    uVar21 = uVar18;
    uVar23 = uVar20;
    uVar25 = uVar22;
    if (((ulonglong)puVar12 & 0xf) != 0) {
      puVar13 = (undefined4 *)((ulonglong)puVar12 & 0xfffffffffffffff0);
      puVar1 = (undefined4 *)((longlong)puVar13 + lVar15);
      uVar19 = *puVar1;
      uVar21 = puVar1[1];
      uVar23 = puVar1[2];
      uVar25 = puVar1[3];
      *puVar12 = *puVar14;
      *(undefined4 *)((longlong)param_1 + (param_3 - 0xc)) = uVar18;
      *(undefined4 *)((longlong)param_1 + (param_3 - 8)) = uVar20;
      *(undefined4 *)((longlong)param_1 + (param_3 - 4)) = uVar22;
      uVar16 = (longlong)puVar13 - (longlong)param_1;
    }
    uVar17 = uVar16 >> 6;
    if (uVar17 != 0) {
      uVar16 = uVar16 & 0x3f;
      puVar14 = puVar13;
      uVar18 = uVar19;
      uVar20 = uVar21;
      uVar22 = uVar23;
      uVar24 = uVar25;
      do {
        puVar10 = (undefined8 *)((longlong)puVar14 + lVar15 + -0x10);
        uVar3 = *puVar10;
        uVar4 = puVar10[1];
        puVar10 = (undefined8 *)((longlong)puVar14 + lVar15 + -0x20);
        uVar5 = *puVar10;
        uVar6 = puVar10[1];
        puVar10 = (undefined8 *)((longlong)puVar14 + lVar15 + -0x30);
        uVar7 = *puVar10;
        uVar8 = puVar10[1];
        puVar13 = (undefined4 *)((longlong)puVar14 + lVar15 + -0x40);
        uVar19 = *puVar13;
        uVar21 = puVar13[1];
        uVar23 = puVar13[2];
        uVar25 = puVar13[3];
        *puVar14 = uVar18;
        puVar14[1] = uVar20;
        puVar14[2] = uVar22;
        puVar14[3] = uVar24;
        puVar13 = puVar14 + -0x10;
        uVar17 = uVar17 - 1;
        *(undefined8 *)(puVar14 + -4) = uVar3;
        *(undefined8 *)(puVar14 + -2) = uVar4;
        *(undefined8 *)(puVar14 + -8) = uVar5;
        *(undefined8 *)(puVar14 + -6) = uVar6;
        *(undefined8 *)(puVar14 + -0xc) = uVar7;
        *(undefined8 *)(puVar14 + -10) = uVar8;
        puVar14 = puVar13;
        uVar18 = uVar19;
        uVar20 = uVar21;
        uVar22 = uVar23;
        uVar24 = uVar25;
      } while (uVar17 != 0);
    }
    for (uVar17 = uVar16 >> 4; uVar17 != 0; uVar17 = uVar17 - 1) {
      *puVar13 = uVar19;
      puVar13[1] = uVar21;
      puVar13[2] = uVar23;
      puVar13[3] = uVar25;
      puVar14 = (undefined4 *)((longlong)puVar13 + lVar15 + -0x10);
      uVar19 = *puVar14;
      uVar21 = puVar14[1];
      uVar23 = puVar14[2];
      uVar25 = puVar14[3];
      puVar13 = puVar13 + -4;
    }
    if ((uVar16 & 0xf) != 0) {
      puVar10 = (undefined8 *)((longlong)puVar13 - (uVar16 & 0xf));
      uVar3 = ((undefined8 *)((longlong)puVar10 + lVar15))[1];
      *puVar10 = *(undefined8 *)((longlong)puVar10 + lVar15);
      puVar10[1] = uVar3;
    }
    *puVar13 = uVar19;
    puVar13[1] = uVar21;
    puVar13[2] = uVar23;
    puVar13[3] = uVar25;
    return;
  }
  puVar14 = (undefined4 *)((longlong)param_1 + lVar15);
  uVar18 = puVar14[1];
  uVar20 = puVar14[2];
  uVar22 = puVar14[3];
  puVar10 = param_1 + 2;
  uVar19 = *puVar14;
  uVar21 = uVar18;
  uVar23 = uVar20;
  uVar25 = uVar22;
  if (((ulonglong)puVar10 & 0xf) != 0) {
    puVar13 = (undefined4 *)(((ulonglong)puVar10 & 0xfffffffffffffff0) + lVar15);
    uVar19 = *puVar13;
    uVar21 = puVar13[1];
    uVar23 = puVar13[2];
    uVar25 = puVar13[3];
    *(undefined4 *)param_1 = *puVar14;
    *(undefined4 *)((longlong)param_1 + 4) = uVar18;
    *(undefined4 *)(param_1 + 1) = uVar20;
    *(undefined4 *)((longlong)param_1 + 0xc) = uVar22;
    puVar10 = (undefined8 *)(((ulonglong)puVar10 & 0xfffffffffffffff0) + 0x10);
  }
  uVar16 = (longlong)param_1 + (param_3 - (longlong)puVar10);
  uVar17 = uVar16 >> 6;
  if (uVar17 != 0) {
    if (uVar17 < 0x1001) {
      uVar16 = uVar16 & 0x3f;
      puVar11 = puVar10;
      uVar18 = uVar19;
      uVar20 = uVar21;
      uVar22 = uVar23;
      uVar24 = uVar25;
      do {
        uVar3 = *(undefined8 *)((longlong)puVar11 + lVar15);
        uVar4 = ((undefined8 *)((longlong)puVar11 + lVar15))[1];
        puVar10 = (undefined8 *)((longlong)puVar11 + lVar15 + 0x10);
        uVar5 = *puVar10;
        uVar6 = puVar10[1];
        puVar10 = (undefined8 *)((longlong)puVar11 + lVar15 + 0x20);
        uVar7 = *puVar10;
        uVar8 = puVar10[1];
        puVar14 = (undefined4 *)((longlong)puVar11 + lVar15 + 0x30);
        uVar19 = *puVar14;
        uVar21 = puVar14[1];
        uVar23 = puVar14[2];
        uVar25 = puVar14[3];
        *(undefined4 *)(puVar11 + -2) = uVar18;
        *(undefined4 *)((longlong)puVar11 + -0xc) = uVar20;
        *(undefined4 *)(puVar11 + -1) = uVar22;
        *(undefined4 *)((longlong)puVar11 + -4) = uVar24;
        puVar10 = puVar11 + 8;
        uVar17 = uVar17 - 1;
        *puVar11 = uVar3;
        puVar11[1] = uVar4;
        puVar11[2] = uVar5;
        puVar11[3] = uVar6;
        puVar11[4] = uVar7;
        puVar11[5] = uVar8;
        puVar11 = puVar10;
        uVar18 = uVar19;
        uVar20 = uVar21;
        uVar22 = uVar23;
        uVar24 = uVar25;
      } while (uVar17 != 0);
    }
    else {
      uVar17 = uVar16 >> 6;
      uVar16 = uVar16 & 0x3f;
      puVar11 = puVar10;
      uVar18 = uVar19;
      uVar20 = uVar21;
      uVar22 = uVar23;
      uVar24 = uVar25;
      do {
        uVar3 = *(undefined8 *)((longlong)puVar11 + lVar15);
        uVar4 = ((undefined8 *)((longlong)puVar11 + lVar15))[1];
        puVar10 = (undefined8 *)((longlong)puVar11 + lVar15 + 0x10);
        uVar5 = *puVar10;
        uVar6 = puVar10[1];
        puVar10 = (undefined8 *)((longlong)puVar11 + lVar15 + 0x20);
        uVar7 = *puVar10;
        uVar8 = puVar10[1];
        puVar14 = (undefined4 *)((longlong)puVar11 + lVar15 + 0x30);
        uVar19 = *puVar14;
        uVar21 = puVar14[1];
        uVar23 = puVar14[2];
        uVar25 = puVar14[3];
        *(undefined4 *)(puVar11 + -2) = uVar18;
        *(undefined4 *)((longlong)puVar11 + -0xc) = uVar20;
        *(undefined4 *)(puVar11 + -1) = uVar22;
        *(undefined4 *)((longlong)puVar11 + -4) = uVar24;
        puVar10 = puVar11 + 8;
        uVar17 = uVar17 - 1;
        *puVar11 = uVar3;
        puVar11[1] = uVar4;
        puVar11[2] = uVar5;
        puVar11[3] = uVar6;
        puVar11[4] = uVar7;
        puVar11[5] = uVar8;
        puVar11 = puVar10;
        uVar18 = uVar19;
        uVar20 = uVar21;
        uVar22 = uVar23;
        uVar24 = uVar25;
      } while (uVar17 != 0);
    }
  }
  for (uVar17 = uVar16 >> 4; uVar17 != 0; uVar17 = uVar17 - 1) {
    *(undefined4 *)(puVar10 + -2) = uVar19;
    *(undefined4 *)((longlong)puVar10 + -0xc) = uVar21;
    *(undefined4 *)(puVar10 + -1) = uVar23;
    *(undefined4 *)((longlong)puVar10 + -4) = uVar25;
    puVar14 = (undefined4 *)((longlong)puVar10 + lVar15);
    uVar19 = *puVar14;
    uVar21 = puVar14[1];
    uVar23 = puVar14[2];
    uVar25 = puVar14[3];
    puVar10 = puVar10 + 2;
  }
  if ((uVar16 & 0xf) != 0) {
    puVar11 = (undefined8 *)((longlong)puVar10 + ((uVar16 & 0xf) - 0x10));
    uVar3 = ((undefined8 *)((longlong)puVar11 + lVar15))[1];
    *puVar11 = *(undefined8 *)((longlong)puVar11 + lVar15);
    puVar11[1] = uVar3;
  }
  *(undefined4 *)(puVar10 + -2) = uVar19;
  *(undefined4 *)((longlong)puVar10 + -0xc) = uVar21;
  *(undefined4 *)(puVar10 + -1) = uVar23;
  *(undefined4 *)((longlong)puVar10 + -4) = uVar25;
  return;
}



// === FUN_140007940 @ 140007940 (size=236) ===

longlong * FUN_140007940(longlong *param_1,byte param_2,undefined1 *param_3)

{
  longlong *plVar1;
  longlong *plVar2;
  undefined4 uVar3;
  longlong lVar4;
  undefined1 *puVar5;
  ulonglong uVar6;
  longlong lVar7;
  longlong *plVar8;
  longlong extraout_XMM0_Qa;
  longlong extraout_XMM0_Qb;
  
  plVar1 = param_1;
  lVar4 = (ulonglong)param_2 * 0x101010101010101;
  if (param_3 < (undefined1 *)0x40) {
    if (param_3 < (undefined1 *)0x10) {
      if ((undefined1 *)0x3 < param_3) {
        uVar3 = (undefined4)lVar4;
        *(undefined4 *)param_1 = uVar3;
        uVar6 = ((ulonglong)param_3 & 8) >> 1;
        *(undefined4 *)(param_3 + -4 + (longlong)param_1) = uVar3;
        *(undefined4 *)((longlong)param_1 + uVar6) = uVar3;
        *(undefined4 *)((longlong)(param_3 + -4 + (longlong)param_1) - uVar6) = uVar3;
        return plVar1;
      }
      if (param_3 != (undefined1 *)0x0) {
        *(char *)param_1 = (char)lVar4;
        if (param_3 != (undefined1 *)0x1) {
          *(short *)(param_3 + (longlong)param_1 + -2) = (short)lVar4;
        }
      }
      return plVar1;
    }
  }
  else {
    if (((DAT_140012c00 & 2) != 0) && ((undefined1 *)0x31f < param_3)) {
      lVar7 = lVar4;
      if ((DAT_140012c00 & 1) == 0) {
        param_1 = (longlong *)FUN_140007b00();
        lVar4 = extraout_XMM0_Qa;
        lVar7 = extraout_XMM0_Qb;
      }
      *plVar1 = lVar4;
      plVar1[1] = lVar7;
      plVar1[2] = lVar4;
      plVar1[3] = lVar7;
      plVar1[4] = lVar4;
      plVar1[5] = lVar7;
      plVar1[6] = lVar4;
      plVar1[7] = lVar7;
      puVar5 = (undefined1 *)((ulonglong)(plVar1 + 8) & 0xffffffffffffffc0);
      for (lVar7 = (longlong)(param_3 + (longlong)plVar1) -
                   (longlong)((ulonglong)(plVar1 + 8) & 0xffffffffffffffc0); lVar7 != 0;
          lVar7 = lVar7 + -1) {
        *puVar5 = (char)lVar4;
        puVar5 = puVar5 + 1;
      }
      return param_1;
    }
    *param_1 = lVar4;
    param_1[1] = lVar4;
    puVar5 = param_3 + (longlong)param_1;
    param_1 = (longlong *)((ulonglong)(param_1 + 2) & 0xfffffffffffffff0);
    param_3 = puVar5 + -(longlong)param_1;
    if ((undefined1 *)0x3f < param_3) {
      plVar8 = (longlong *)
               ((ulonglong)((longlong)(param_1 + -6) + (longlong)param_3) & 0xfffffffffffffff0);
      uVar6 = (ulonglong)param_3 >> 6;
      plVar2 = param_1;
      do {
        *plVar2 = lVar4;
        plVar2[1] = lVar4;
        plVar2[2] = lVar4;
        plVar2[3] = lVar4;
        uVar6 = uVar6 - 1;
        plVar2[4] = lVar4;
        plVar2[5] = lVar4;
        plVar2[6] = lVar4;
        plVar2[7] = lVar4;
        plVar2 = plVar2 + 8;
      } while (uVar6 != 0);
      *plVar8 = lVar4;
      plVar8[1] = lVar4;
      plVar8[2] = lVar4;
      plVar8[3] = lVar4;
      plVar8[4] = lVar4;
      plVar8[5] = lVar4;
      *(longlong *)((longlong)(param_1 + -2) + (longlong)param_3) = lVar4;
      ((longlong *)((longlong)(param_1 + -2) + (longlong)param_3))[1] = lVar4;
      return plVar1;
    }
  }
  plVar8 = (longlong *)(param_3 + -0x10 + (longlong)param_1);
  *param_1 = lVar4;
  param_1[1] = lVar4;
  uVar6 = ((ulonglong)param_3 & 0x20) >> 1;
  *plVar8 = lVar4;
  plVar8[1] = lVar4;
  plVar2 = (longlong *)((longlong)param_1 + uVar6);
  *plVar2 = lVar4;
  plVar2[1] = lVar4;
  plVar8 = (longlong *)((longlong)plVar8 - uVar6);
  *plVar8 = lVar4;
  plVar8[1] = lVar4;
  return plVar1;
}



// === FUN_140007a80 @ 140007a80 (size=67) ===

undefined8 FUN_140007a80(undefined1 *param_1,undefined8 param_2,longlong param_3)

{
  undefined8 in_RAX;
  undefined1 *puVar1;
  undefined1 *puVar2;
  undefined1 in_XMM0_Ba;
  undefined1 extraout_XMM0_Ba;
  undefined1 in_XMM0_Bb;
  undefined1 extraout_XMM0_Bb;
  undefined1 in_XMM0_Bc;
  undefined1 extraout_XMM0_Bc;
  undefined1 in_XMM0_Bd;
  undefined1 extraout_XMM0_Bd;
  undefined1 in_XMM0_Be;
  undefined1 extraout_XMM0_Be;
  undefined1 in_XMM0_Bf;
  undefined1 extraout_XMM0_Bf;
  undefined1 in_XMM0_Bg;
  undefined1 extraout_XMM0_Bg;
  undefined1 in_XMM0_Bh;
  undefined1 extraout_XMM0_Bh;
  undefined1 in_XMM0_Bi;
  undefined1 extraout_XMM0_Bi;
  undefined1 in_XMM0_Bj;
  undefined1 extraout_XMM0_Bj;
  undefined1 in_XMM0_Bk;
  undefined1 extraout_XMM0_Bk;
  undefined1 in_XMM0_Bl;
  undefined1 extraout_XMM0_Bl;
  undefined1 in_XMM0_Bm;
  undefined1 extraout_XMM0_Bm;
  undefined1 in_XMM0_Bn;
  undefined1 extraout_XMM0_Bn;
  undefined1 in_XMM0_Bo;
  undefined1 extraout_XMM0_Bo;
  undefined1 in_XMM0_Bp;
  undefined1 extraout_XMM0_Bp;
  
  if ((DAT_140012c00 & 1) == 0) {
    in_RAX = FUN_140007b00();
    in_XMM0_Ba = extraout_XMM0_Ba;
    in_XMM0_Bb = extraout_XMM0_Bb;
    in_XMM0_Bc = extraout_XMM0_Bc;
    in_XMM0_Bd = extraout_XMM0_Bd;
    in_XMM0_Be = extraout_XMM0_Be;
    in_XMM0_Bf = extraout_XMM0_Bf;
    in_XMM0_Bg = extraout_XMM0_Bg;
    in_XMM0_Bh = extraout_XMM0_Bh;
    in_XMM0_Bi = extraout_XMM0_Bi;
    in_XMM0_Bj = extraout_XMM0_Bj;
    in_XMM0_Bk = extraout_XMM0_Bk;
    in_XMM0_Bl = extraout_XMM0_Bl;
    in_XMM0_Bm = extraout_XMM0_Bm;
    in_XMM0_Bn = extraout_XMM0_Bn;
    in_XMM0_Bo = extraout_XMM0_Bo;
    in_XMM0_Bp = extraout_XMM0_Bp;
  }
  *param_1 = in_XMM0_Ba;
  param_1[1] = in_XMM0_Bb;
  param_1[2] = in_XMM0_Bc;
  param_1[3] = in_XMM0_Bd;
  param_1[4] = in_XMM0_Be;
  param_1[5] = in_XMM0_Bf;
  param_1[6] = in_XMM0_Bg;
  param_1[7] = in_XMM0_Bh;
  param_1[8] = in_XMM0_Bi;
  param_1[9] = in_XMM0_Bj;
  param_1[10] = in_XMM0_Bk;
  param_1[0xb] = in_XMM0_Bl;
  param_1[0xc] = in_XMM0_Bm;
  param_1[0xd] = in_XMM0_Bn;
  param_1[0xe] = in_XMM0_Bo;
  param_1[0xf] = in_XMM0_Bp;
  param_1[0x10] = in_XMM0_Ba;
  param_1[0x11] = in_XMM0_Bb;
  param_1[0x12] = in_XMM0_Bc;
  param_1[0x13] = in_XMM0_Bd;
  param_1[0x14] = in_XMM0_Be;
  param_1[0x15] = in_XMM0_Bf;
  param_1[0x16] = in_XMM0_Bg;
  param_1[0x17] = in_XMM0_Bh;
  param_1[0x18] = in_XMM0_Bi;
  param_1[0x19] = in_XMM0_Bj;
  param_1[0x1a] = in_XMM0_Bk;
  param_1[0x1b] = in_XMM0_Bl;
  param_1[0x1c] = in_XMM0_Bm;
  param_1[0x1d] = in_XMM0_Bn;
  param_1[0x1e] = in_XMM0_Bo;
  param_1[0x1f] = in_XMM0_Bp;
  param_1[0x20] = in_XMM0_Ba;
  param_1[0x21] = in_XMM0_Bb;
  param_1[0x22] = in_XMM0_Bc;
  param_1[0x23] = in_XMM0_Bd;
  param_1[0x24] = in_XMM0_Be;
  param_1[0x25] = in_XMM0_Bf;
  param_1[0x26] = in_XMM0_Bg;
  param_1[0x27] = in_XMM0_Bh;
  param_1[0x28] = in_XMM0_Bi;
  param_1[0x29] = in_XMM0_Bj;
  param_1[0x2a] = in_XMM0_Bk;
  param_1[0x2b] = in_XMM0_Bl;
  param_1[0x2c] = in_XMM0_Bm;
  param_1[0x2d] = in_XMM0_Bn;
  param_1[0x2e] = in_XMM0_Bo;
  param_1[0x2f] = in_XMM0_Bp;
  param_1[0x30] = in_XMM0_Ba;
  param_1[0x31] = in_XMM0_Bb;
  param_1[0x32] = in_XMM0_Bc;
  param_1[0x33] = in_XMM0_Bd;
  param_1[0x34] = in_XMM0_Be;
  param_1[0x35] = in_XMM0_Bf;
  param_1[0x36] = in_XMM0_Bg;
  param_1[0x37] = in_XMM0_Bh;
  param_1[0x38] = in_XMM0_Bi;
  param_1[0x39] = in_XMM0_Bj;
  param_1[0x3a] = in_XMM0_Bk;
  param_1[0x3b] = in_XMM0_Bl;
  param_1[0x3c] = in_XMM0_Bm;
  param_1[0x3d] = in_XMM0_Bn;
  param_1[0x3e] = in_XMM0_Bo;
  param_1[0x3f] = in_XMM0_Bp;
  puVar1 = (undefined1 *)((ulonglong)(param_1 + 0x40) & 0xffffffffffffffc0);
  for (puVar2 = param_1 + (param_3 - (longlong)((ulonglong)(param_1 + 0x40) & 0xffffffffffffffc0));
      puVar2 != (undefined1 *)0x0; puVar2 = puVar2 + -1) {
    *puVar1 = in_XMM0_Ba;
    puVar1 = puVar1 + 1;
  }
  return in_RAX;
}



// === FUN_140007b00 @ 140007b00 (size=38) ===

undefined8 FUN_140007b00(void)

{
  undefined8 in_RAX;
  
  FUN_140006fe0();
  return in_RAX;
}



// === FUN_140015000 @ 140015000 (size=65) ===

void FUN_140015000(undefined8 param_1,undefined8 param_2)

{
  PcAddAdapterDevice(param_1,param_2,FUN_140015520,4,0);
                    /* WARNING: Could not recover jumptable at 0x00014001503a. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  IoRegisterDeviceInterface(param_2,&DAT_1400088d0,0,&DAT_140012c88);
  return;
}



// === FUN_140015044 @ 140015044 (size=111) ===

undefined8 FUN_140015044(undefined8 param_1,undefined8 param_2,longlong *param_3)

{
  undefined8 uVar1;
  
  uVar1 = FUN_140015124(0,param_1,param_2,param_3,PTR_DAT_14000ead8);
  if (-1 < (int)uVar1) {
    uVar1 = 0;
  }
  return uVar1;
}



// === FUN_1400150b4 @ 1400150b4 (size=111) ===

int FUN_1400150b4(undefined8 param_1,undefined8 param_2,longlong *param_3)

{
  int iVar1;
  
  iVar1 = FUN_140015168(0,param_1,param_2,param_3,PTR_DAT_14000ead0);
  if (-1 < iVar1) {
    iVar1 = 0;
  }
  return iVar1;
}



// === FUN_140015124 @ 140015124 (size=66) ===

void FUN_140015124(undefined4 param_1,undefined8 param_2,undefined8 param_3,longlong *param_4,
                  undefined8 param_5)

{
  (*(code *)PTR__guard_dispatch_icall_140008188)(param_4,param_1,param_3,param_5,0,0,0,0,0);
  return;
}



// === FUN_140015168 @ 140015168 (size=636) ===

/* WARNING: Type propagation algorithm not settling */

int FUN_140015168(undefined4 param_1,undefined8 param_2,undefined8 param_3,longlong *param_4,
                 undefined8 param_5)

{
  int iVar1;
  int iVar2;
  longlong local_res20;
  undefined8 local_98;
  undefined8 local_90;
  undefined8 local_88;
  undefined8 local_80;
  undefined8 local_78;
  longlong local_70 [3];
  undefined8 local_58;
  void *local_50;
  undefined8 local_48;
  undefined8 local_40;
  undefined8 local_38;
  undefined8 local_30;
  void *local_28;
  undefined8 local_20;
  
  local_70[0] = 0;
  local_res20 = 0;
  local_78 = 0;
  local_88 = 0;
  local_80 = 0;
  iVar1 = (*(code *)PTR__guard_dispatch_icall_140008188)
                    (param_4,param_1,param_3,param_5,0,local_70,&local_res20,0,0);
  if (local_res20 != 0) {
    iVar1 = (*(code *)PTR__guard_dispatch_icall_140008188)(local_res20,&DAT_140008980,&local_78);
    if (-1 < iVar1) {
      (*(code *)PTR__guard_dispatch_icall_140008188)(param_4,local_78);
      (*(code *)PTR__guard_dispatch_icall_140008188)();
    }
    iVar1 = (*(code *)PTR__guard_dispatch_icall_140008188)(local_res20,&DAT_1400089b0,&local_88);
    if (-1 < iVar1) {
      local_98 = 0;
      local_90 = 0;
      PcGetPhysicalDeviceObject(param_2,&local_90);
      local_70[2] = local_90;
      local_70[1] = 0x28;
      local_58 = 1;
      local_48 = 0;
      local_50 = SystemReserved1[0xf];
      iVar1 = (*(code *)PTR__guard_dispatch_icall_140008188)(local_88,0,local_70 + 1,&local_98);
      if (-1 < iVar1) {
        (*(code *)PTR__guard_dispatch_icall_140008188)(local_88,local_98);
        local_98 = 0;
      }
      (*(code *)PTR__guard_dispatch_icall_140008188)();
      local_88 = 0;
    }
    iVar1 = (*(code *)PTR__guard_dispatch_icall_140008188)(local_res20,&DAT_1400089c0,&local_80);
    if (-1 < iVar1) {
      local_90 = 0;
      local_98 = 0;
      PcGetPhysicalDeviceObject(param_2,&local_98);
      local_38 = local_98;
      local_40 = 0x28;
      local_30 = 1;
      local_20 = 0;
      local_28 = SystemReserved1[0xf];
      iVar2 = (*(code *)PTR__guard_dispatch_icall_140008188)
                        (local_80,local_98,0,&local_40,&local_90);
      if (-1 < iVar2) {
        (*(code *)PTR__guard_dispatch_icall_140008188)(local_80,local_90);
        local_90 = 0;
      }
      (*(code *)PTR__guard_dispatch_icall_140008188)();
      local_80 = 0;
    }
  }
  if (local_70[0] != 0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)(local_70[0]);
    local_70[0] = 0;
  }
  if (local_res20 != 0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)();
  }
  return iVar1;
}



// === FUN_1400153f0 @ 1400153f0 (size=63) ===

undefined8 FUN_1400153f0(undefined8 param_1,longlong param_2)

{
  longlong lVar1;
  
  lVar1 = *(longlong *)(*(longlong *)(*(longlong *)(param_2 + 0xb8) + 0x30) + 0x18);
  *(undefined4 *)(param_2 + 0x30) = 0;
  *(undefined8 *)(param_2 + 0x38) = 0;
  if (lVar1 == 0x7b) {
    IofCompleteRequest(param_2,0);
  }
  else {
    PcDispatchIrp();
  }
  return 0;
}



// === FUN_140015430 @ 140015430 (size=81) ===

undefined4 FUN_140015430(undefined8 param_1,longlong param_2)

{
  undefined4 uVar1;
  
  uVar1 = 0;
  if (*(short *)(*(longlong *)(param_2 + 0xb8) + 0x18) == 0x800) {
    *(undefined8 *)(*(longlong *)(*(longlong *)(param_2 + 0xb8) + 0x30) + 0x18) = 0x7b;
    *(undefined8 *)(param_2 + 0x38) = 0;
    *(undefined4 *)(param_2 + 0x30) = 0;
    IofCompleteRequest(param_2,0);
  }
  else {
    uVar1 = PcDispatchIrp(param_1,param_2);
  }
  return uVar1;
}



// === FUN_140015490 @ 140015490 (size=129) ===

void FUN_140015490(longlong param_1,longlong param_2)

{
  byte bVar1;
  longlong lVar2;
  
  bVar1 = *(byte *)(*(longlong *)(param_2 + 0xb8) + 1);
  if ((bVar1 < 0x18) && ((0x800014U >> (bVar1 & 0x1f) & 1) != 0)) {
    lVar2 = *(longlong *)(param_1 + 0x40);
    if (*(longlong *)(lVar2 + 0x28) != 0) {
      (*(code *)PTR__guard_dispatch_icall_140008188)();
      (*(code *)PTR__guard_dispatch_icall_140008188)();
      *(undefined8 *)(lVar2 + 0x28) = 0;
    }
  }
                    /* WARNING: Could not recover jumptable at 0x00014001550a. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  PcDispatchIrp(param_1,param_2);
  return;
}



// === FUN_140015520 @ 140015520 (size=264) ===

ulonglong FUN_140015520(longlong param_1,undefined8 param_2)

{
  longlong lVar1;
  uint uVar2;
  ulonglong uVar3;
  ulonglong uVar4;
  longlong *local_res8;
  longlong local_res20;
  
  lVar1 = *(longlong *)(param_1 + 0x40);
  local_res8 = (longlong *)0x0;
  local_res20 = 0;
  uVar3 = FUN_1400171d8(&local_res20,&DAT_140008a40,(undefined8 *)0x0,0x40);
  uVar4 = uVar3 & 0xffffffff;
  if (-1 < (int)uVar3) {
    uVar2 = (*(code *)PTR__guard_dispatch_icall_140008188)(local_res20,&DAT_140008a40,&local_res8);
    uVar4 = (ulonglong)uVar2;
    if (-1 < (int)uVar2) {
      uVar2 = (*(code *)PTR__guard_dispatch_icall_140008188)(local_res8,param_1);
      uVar4 = (ulonglong)uVar2;
      if (-1 < (int)uVar2) {
        uVar2 = PcRegisterAdapterPowerManagement(local_res8,param_1);
        uVar4 = (ulonglong)uVar2;
        if (-1 < (int)uVar2) {
          uVar2 = FUN_1400150b4(param_1,param_2,local_res8);
          uVar4 = (ulonglong)uVar2;
          if (-1 < (int)uVar2) {
            uVar3 = FUN_140015044(param_1,param_2,local_res8);
            uVar4 = uVar3 & 0xffffffff;
            if (-1 < (int)uVar3) {
              uVar2 = IoSetDeviceInterfaceState(&DAT_140012c88,1);
              uVar4 = (ulonglong)uVar2;
            }
          }
        }
      }
    }
  }
  if (local_res8 != (longlong *)0x0) {
    *(longlong **)(lVar1 + 0x28) = local_res8;
  }
  if (local_res20 != 0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)();
  }
  return uVar4;
}



// === FUN_140015630 @ 140015630 (size=60) ===

void FUN_140015630(longlong param_1)

{
  if (param_1 != 0) {
    if (DAT_140012c98 != 0) {
      (*(code *)PTR__guard_dispatch_icall_140008188)();
    }
    if (*DAT_140013210 != 0) {
      (*(code *)PTR__guard_dispatch_icall_140008188)();
    }
  }
  return;
}



// === FUN_14001566c @ 14001566c (size=291) ===

void FUN_14001566c(undefined8 *param_1)

{
  *param_1 = &PTR_FUN_140008c90;
  param_1[1] = &PTR_FUN_140008da0;
  param_1[2] = &PTR_FUN_140008dd0;
  if (param_1[10] != 0) {
    FUN_1400048b8(param_1[10]);
    param_1[10] = 0;
  }
  if (param_1[0xb] != 0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)();
    param_1[0xb] = 0;
  }
  if (param_1[5] != 0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)();
    param_1[5] = 0;
  }
  if (param_1[8] != 0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)(DAT_140013210);
    param_1[8] = 0;
  }
  FUN_1400010bc(DAT_140012e60);
  DAT_140012e60 = 0;
  FUN_1400010bc(DAT_140012e68);
  DAT_140012e68 = 0;
  if (DAT_140012f30 != 0) {
    FUN_140003a54(DAT_140012f30);
    DAT_140012f30 = 0;
  }
  if (DAT_140012f40 != 0) {
    FUN_140003a54(DAT_140012f40);
    DAT_140012f40 = 0;
  }
  LOCK();
  DAT_140012f70 = DAT_140012f70 + -1;
  UNLOCK();
  FUN_140006e20(param_1 + 2);
  return;
}



// === FUN_140015790 @ 140015790 (size=370) ===

uint FUN_140015790(longlong param_1,longlong param_2,longlong *param_3,longlong *param_4)

{
  short sVar1;
  code *pcVar2;
  longlong *plVar3;
  longlong *plVar4;
  longlong *plVar5;
  longlong lVar6;
  uint uVar7;
  longlong lVar8;
  
  plVar3 = (longlong *)FUN_1400048a8((undefined1 *)0x238,0x40);
  if (plVar3 == (longlong *)0x0) {
    uVar7 = 0xc000009a;
  }
  else {
    FUN_140007940(plVar3,0,(undefined1 *)0x238);
    plVar5 = plVar3 + 2;
    lVar6 = 0x104;
    lVar8 = param_2 - (longlong)plVar5;
    do {
      if ((lVar6 == -0x7ffffefa) || (sVar1 = *(short *)(lVar8 + (longlong)plVar5), sVar1 == 0))
      break;
      *(short *)plVar5 = sVar1;
      plVar5 = (longlong *)((longlong)plVar5 + 2);
      lVar6 = lVar6 + -1;
    } while (lVar6 != 0);
    plVar4 = (longlong *)((longlong)plVar5 + -2);
    if (lVar6 != 0) {
      plVar4 = plVar5;
    }
    *(short *)plVar4 = 0;
    uVar7 = ~-(uint)(lVar6 != 0) & 0x80000005;
    if (lVar6 == 0) {
      FUN_1400048b8((longlong)plVar3);
    }
    else {
      plVar3[0x43] = (longlong)param_3;
      (*(code *)PTR__guard_dispatch_icall_140008188)(param_3);
      plVar3[0x44] = (longlong)param_4;
      (*(code *)PTR__guard_dispatch_icall_140008188)(param_4);
      (*(code *)PTR__guard_dispatch_icall_140008188)(param_4,&DAT_1400089d0,plVar3 + 0x45);
      (*(code *)PTR__guard_dispatch_icall_140008188)(param_4,&DAT_140008a50,plVar3 + 0x46);
      lVar6 = param_1 + 0x68;
      plVar5 = *(longlong **)(param_1 + 0x70);
      if (*plVar5 != lVar6) {
        plVar5 = (longlong *)0x3;
        pcVar2 = (code *)swi(0x29);
        lVar6 = (*pcVar2)();
      }
      *plVar3 = lVar6;
      plVar3[1] = (longlong)plVar5;
      *plVar5 = (longlong)plVar3;
      *(longlong **)(lVar6 + 8) = plVar3;
    }
  }
  return uVar7;
}



// === thunk_FUN_1400160cc @ 140015910 (size=5) ===

void thunk_FUN_1400160cc(longlong param_1)

{
  longlong *plVar1;
  longlong lVar2;
  longlong *plVar3;
  code *pcVar4;
  
  plVar1 = (longlong *)(param_1 + 0x68);
  while( true ) {
    plVar3 = (longlong *)*plVar1;
    if (plVar3 == plVar1) {
      return;
    }
    if (((longlong *)plVar3[1] != plVar1) || (lVar2 = *plVar3, *(longlong **)(lVar2 + 8) != plVar3))
    break;
    *plVar1 = lVar2;
    *(longlong **)(lVar2 + 8) = plVar1;
    if (plVar3[0x43] != 0) {
      (*(code *)PTR__guard_dispatch_icall_140008188)();
      plVar3[0x43] = 0;
    }
    if (plVar3[0x44] != 0) {
      (*(code *)PTR__guard_dispatch_icall_140008188)();
      plVar3[0x44] = 0;
    }
    if (plVar3[0x46] != 0) {
      (*(code *)PTR__guard_dispatch_icall_140008188)();
      plVar3[0x46] = 0;
    }
    if (plVar3[0x45] != 0) {
      (*(code *)PTR__guard_dispatch_icall_140008188)();
      plVar3[0x45] = 0;
    }
    FUN_140007940(plVar3 + 2,0,(undefined1 *)0x208);
    FUN_1400048b8((longlong)plVar3);
  }
  pcVar4 = (code *)swi(0x29);
  (*pcVar4)(3);
  pcVar4 = (code *)swi(3);
  (*pcVar4)();
  return;
}



// === FUN_140015920 @ 140015920 (size=194) ===

int FUN_140015920(longlong *param_1,undefined8 param_2,undefined8 param_3,longlong param_4,
                 uint param_5)

{
  undefined4 uVar1;
  undefined4 uVar2;
  int iVar3;
  undefined8 uVar4;
  undefined4 *puVar5;
  uint uVar6;
  undefined8 uVar7;
  
  iVar3 = 0;
  uVar6 = 0;
  if (param_5 != 0) {
    puVar5 = (undefined4 *)(param_4 + 4);
    do {
      if (iVar3 < 0) goto LAB_1400159a3;
      if (puVar5[1] == 0) {
        uVar2 = *puVar5;
        uVar1 = puVar5[-1];
        uVar4 = param_2;
        uVar7 = param_3;
LAB_140015985:
        iVar3 = PcRegisterPhysicalConnection(param_1[6],uVar4,uVar1,uVar7,uVar2);
      }
      else if (puVar5[1] == 1) {
        uVar2 = puVar5[-1];
        uVar1 = *puVar5;
        uVar4 = param_3;
        uVar7 = param_2;
        goto LAB_140015985;
      }
      uVar6 = uVar6 + 1;
      puVar5 = puVar5 + 3;
    } while (uVar6 < param_5);
    if (iVar3 < 0) {
LAB_1400159a3:
      (*(code *)PTR__guard_dispatch_icall_140008188)(param_1,param_2,param_3,param_4,param_5);
    }
  }
  return iVar3;
}



// === FUN_1400159e4 @ 1400159e4 (size=765) ===

int FUN_1400159e4(longlong param_1,longlong param_2,int param_3)

{
  short sVar1;
  int iVar2;
  longlong *plVar3;
  longlong *plVar4;
  longlong *plVar5;
  longlong *plVar6;
  ulonglong uVar7;
  longlong lVar8;
  ulonglong uVar9;
  longlong *plVar10;
  uint uVar11;
  uint local_res8 [2];
  undefined4 local_res20 [2];
  uint *puVar12;
  longlong local_b8;
  longlong local_b0;
  undefined1 local_a8 [16];
  undefined4 local_98;
  longlong local_90;
  undefined1 *local_88;
  undefined4 local_80;
  undefined8 local_78;
  undefined8 uStack_70;
  undefined4 local_68 [2];
  longlong local_60;
  undefined1 *local_58;
  undefined4 local_50;
  undefined8 local_48;
  undefined8 uStack_40;
  
  plVar4 = (longlong *)0x0;
  local_res8[0] = 0;
  local_res20[0] = 0;
  local_b8 = 0;
  local_b0 = 0;
  if ((param_1 == 0) || (param_2 == 0)) {
    iVar2 = -0x3ffffff3;
  }
  else {
    uVar11 = 0x118;
    plVar3 = FUN_140003a04(0x40,(undefined1 *)0x118);
    plVar10 = plVar4;
    if (plVar3 == (longlong *)0x0) {
      iVar2 = -0x3fffff66;
    }
    else {
      while( true ) {
        puVar12 = local_res8;
        iVar2 = ZwEnumerateKey(param_1,plVar10,0,plVar3,uVar11,puVar12);
        if (iVar2 == -0x7fffffe6) break;
        if ((iVar2 == -0x3fffffdd) || (iVar2 == -0x7ffffffb)) {
          FUN_140003a54((longlong)plVar3);
          uVar11 = local_res8[0];
          plVar3 = FUN_140003a04(0x40,(undefined1 *)(ulonglong)local_res8[0]);
          if (plVar3 != (longlong *)0x0) {
            puVar12 = local_res8;
            iVar2 = ZwEnumerateKey(param_1,plVar10,0,plVar3,uVar11,puVar12);
            if (iVar2 != -0x7fffffe6) goto LAB_140015a9a;
            break;
          }
          iVar2 = -0x3fffff66;
        }
        else {
LAB_140015a9a:
          if (-1 < iVar2) {
            plVar4 = FUN_140003a04(0x40,(undefined1 *)
                                        ((ulonglong)*(uint *)((longlong)plVar3 + 0xc) + 2));
            uVar9 = (ulonglong)*(uint *)((longlong)plVar3 + 0xc) + 2 >> 1;
            if (uVar9 - 1 < 0x7fffffff) {
              uVar7 = (ulonglong)(*(uint *)((longlong)plVar3 + 0xc) >> 1);
              if (uVar7 < 0x7fffffff) {
                lVar8 = uVar7 - uVar9;
                plVar6 = plVar4;
                do {
                  if ((lVar8 + uVar9 == 0) ||
                     (sVar1 = *(short *)((longlong)plVar3 + (0x10 - (longlong)plVar4) +
                                        (longlong)plVar6), sVar1 == 0)) break;
                  *(short *)plVar6 = sVar1;
                  plVar6 = (longlong *)((longlong)plVar6 + 2);
                  uVar9 = uVar9 - 1;
                } while (uVar9 != 0);
                plVar5 = (longlong *)((longlong)plVar6 + -2);
                if (uVar9 != 0) {
                  plVar5 = plVar6;
                }
                *(short *)plVar5 = 0;
              }
              else {
                *(undefined2 *)plVar4 = 0;
              }
            }
            *(undefined2 *)
             ((longlong)plVar4 + (ulonglong)(*(uint *)((longlong)plVar3 + 0xc) >> 1) * 2) = 0;
            RtlInitUnicodeString(local_a8,plVar4);
            local_88 = local_a8;
            local_98 = 0x30;
            local_80 = 0x240;
            local_78 = 0;
            uStack_70 = 0;
            local_90 = param_1;
            iVar2 = ZwOpenKey(&local_b8,0x20019);
            if (-1 < iVar2) {
              local_58 = local_a8;
              local_68[0] = 0x30;
              local_50 = 0x200;
              local_48 = 0;
              uStack_40 = 0;
              local_60 = param_2;
              iVar2 = ZwCreateKey(&local_b0,0x20006,local_68,0,0,
                                  (ulonglong)puVar12 & 0xffffffff00000000,local_res20);
              if (-1 < iVar2) {
                iVar2 = FUN_1400159e4(local_b8,local_b0,1);
                goto LAB_140015ca1;
              }
            }
            ZwClose(local_b8);
          }
        }
LAB_140015ca1:
        if (plVar4 != (longlong *)0x0) {
          FUN_140003a54((longlong)plVar4);
        }
        if (local_b8 != 0) {
          ZwClose();
        }
        if (local_b0 != 0) {
          ZwClose();
        }
        if (iVar2 < 0) goto LAB_140015b68;
        plVar10 = (longlong *)(ulonglong)((int)plVar10 + 1);
      }
      iVar2 = 0;
      if (param_3 != 0) {
        iVar2 = FUN_140015ce4(param_1,param_2);
      }
LAB_140015b68:
      if (plVar3 != (longlong *)0x0) {
        FUN_140003a54((longlong)plVar3);
      }
    }
  }
  return iVar2;
}



// === FUN_140015ce4 @ 140015ce4 (size=557) ===

int FUN_140015ce4(undefined8 param_1,undefined8 param_2)

{
  short sVar1;
  int iVar2;
  longlong *plVar3;
  longlong *plVar4;
  longlong *plVar5;
  longlong *plVar6;
  ulonglong uVar7;
  longlong lVar8;
  int iVar9;
  ulonglong uVar10;
  uint uVar11;
  uint local_res18 [2];
  uint *puVar12;
  undefined4 uVar13;
  undefined1 local_38 [16];
  
  local_res18[0] = 0;
  uVar11 = 0x118;
  plVar4 = (longlong *)0x0;
  plVar3 = FUN_140003a04(0x40,(undefined1 *)0x118);
  if (plVar3 == (longlong *)0x0) {
    iVar2 = -0x3fffff66;
  }
  else {
    iVar9 = 0;
    while( true ) {
      puVar12 = local_res18;
      iVar2 = ZwEnumerateValueKey(param_1,iVar9,1,plVar3,uVar11,puVar12);
      uVar13 = (undefined4)((ulonglong)puVar12 >> 0x20);
      if (iVar2 == -0x7fffffe6) break;
      if ((iVar2 == -0x3fffffdd) || (iVar2 == -0x7ffffffb)) {
        FUN_140003a54((longlong)plVar3);
        uVar11 = local_res18[0];
        plVar3 = FUN_140003a04(0x40,(undefined1 *)(ulonglong)local_res18[0]);
        if (plVar3 != (longlong *)0x0) {
          puVar12 = local_res18;
          iVar2 = ZwEnumerateValueKey(param_1,iVar9,1,plVar3,uVar11,puVar12);
          uVar13 = (undefined4)((ulonglong)puVar12 >> 0x20);
          if (iVar2 != -0x7fffffe6) goto LAB_140015d7e;
          iVar2 = 0;
          goto LAB_140015ee4;
        }
        iVar2 = -0x3fffff66;
      }
      else {
LAB_140015d7e:
        if (-1 < iVar2) {
          plVar4 = FUN_140003a04(0x40,(undefined1 *)((ulonglong)*(uint *)(plVar3 + 2) + 4));
          uVar10 = (ulonglong)*(uint *)(plVar3 + 2) + 4 >> 1;
          if (uVar10 - 1 < 0x7fffffff) {
            uVar7 = (ulonglong)(*(uint *)(plVar3 + 2) >> 1);
            if (uVar7 < 0x7fffffff) {
              lVar8 = uVar7 - uVar10;
              plVar6 = plVar4;
              do {
                if ((lVar8 + uVar10 == 0) ||
                   (sVar1 = *(short *)((longlong)plVar3 + (0x14 - (longlong)plVar4) +
                                      (longlong)plVar6), sVar1 == 0)) break;
                *(short *)plVar6 = sVar1;
                plVar6 = (longlong *)((longlong)plVar6 + 2);
                uVar10 = uVar10 - 1;
              } while (uVar10 != 0);
              plVar5 = (longlong *)((longlong)plVar6 + -2);
              if (uVar10 != 0) {
                plVar5 = plVar6;
              }
              *(short *)plVar5 = 0;
            }
            else {
              *(undefined2 *)plVar4 = 0;
            }
          }
          *(undefined2 *)((longlong)plVar4 + (ulonglong)(*(uint *)(plVar3 + 2) >> 1) * 2) = 0;
          RtlInitUnicodeString(local_38,plVar4);
          iVar2 = ZwSetValueKey(param_2,local_38,0,*(undefined4 *)((longlong)plVar3 + 4),
                                (ulonglong)*(uint *)(plVar3 + 1) + (longlong)plVar3,
                                CONCAT44(uVar13,*(undefined4 *)((longlong)plVar3 + 0xc)));
        }
      }
      if (plVar4 != (longlong *)0x0) {
        FUN_140003a54((longlong)plVar4);
      }
      if (iVar2 < 0) goto LAB_140015edf;
      iVar9 = iVar9 + 1;
    }
    iVar2 = 0;
LAB_140015edf:
    if (plVar3 != (longlong *)0x0) {
LAB_140015ee4:
      FUN_140003a54((longlong)plVar3);
    }
  }
  return iVar2;
}



// === FUN_140015f14 @ 140015f14 (size=195) ===

ulonglong FUN_140015f14(longlong *param_1,undefined8 param_2,longlong param_3,uint param_4,
                       longlong param_5,undefined8 *param_6)

{
  uint uVar1;
  undefined8 uVar2;
  ulonglong uVar3;
  ulonglong uVar4;
  undefined1 local_18 [16];
  
  RtlInitUnicodeString(local_18);
  *param_6 = 0;
  param_6[1] = 0;
  uVar2 = (*(code *)PTR__guard_dispatch_icall_140008188)(param_1);
  uVar1 = IoRegisterDeviceInterface(uVar2,&DAT_140008430,local_18,param_6);
  uVar4 = (ulonglong)uVar1;
  if (-1 < (int)uVar1) {
    if (param_3 != 0) {
      uVar1 = FUN_140016e10(param_1,param_6,param_3);
      uVar4 = (ulonglong)uVar1;
      if ((int)uVar1 < 0) goto LAB_140015fab;
    }
    uVar3 = FUN_14001773c(param_6,param_4,param_5);
    uVar4 = uVar3 & 0xffffffff;
    if (-1 < (int)uVar3) {
      return 0;
    }
  }
LAB_140015fab:
  RtlFreeUnicodeString(param_6);
  *param_6 = 0;
  param_6[1] = 0;
  return uVar4;
}



// === FUN_140015fe0 @ 140015fe0 (size=233) ===

int FUN_140015fe0(longlong param_1,undefined8 *param_2,undefined8 *param_3,longlong param_4,
                 uint param_5)

{
  undefined4 uVar1;
  int iVar2;
  int iVar3;
  ulonglong uVar4;
  undefined4 *puVar5;
  undefined8 *puVar6;
  longlong local_res10;
  undefined8 *puVar7;
  undefined4 uVar8;
  
  local_res10 = 0;
  iVar2 = (*(code *)PTR__guard_dispatch_icall_140008188)(param_2,&DAT_140008a10,&local_res10);
  if ((-1 < iVar2) && (param_5 != 0)) {
    puVar5 = (undefined4 *)(param_4 + 4);
    uVar4 = (ulonglong)param_5;
    iVar3 = iVar2;
    do {
      if (puVar5[1] == 0) {
        uVar8 = *puVar5;
        uVar1 = puVar5[-1];
        puVar6 = param_2;
        puVar7 = param_3;
LAB_14001606c:
        iVar3 = (*(code *)PTR__guard_dispatch_icall_140008188)
                          (local_res10,*(undefined8 *)(param_1 + 0x30),puVar6,uVar1,puVar7,uVar8);
      }
      else if (puVar5[1] == 1) {
        uVar8 = puVar5[-1];
        uVar1 = *puVar5;
        puVar6 = param_3;
        puVar7 = param_2;
        goto LAB_14001606c;
      }
      iVar2 = 0;
      if (iVar3 < 0) {
        iVar2 = iVar3;
      }
      puVar5 = puVar5 + 3;
      uVar4 = uVar4 - 1;
      iVar3 = iVar2;
    } while (uVar4 != 0);
  }
  if (local_res10 != 0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)();
  }
  return iVar2;
}



// === FUN_1400160cc @ 1400160cc (size=242) ===

void FUN_1400160cc(longlong param_1)

{
  longlong *plVar1;
  longlong lVar2;
  longlong *plVar3;
  code *pcVar4;
  
  plVar1 = (longlong *)(param_1 + 0x68);
  while( true ) {
    plVar3 = (longlong *)*plVar1;
    if (plVar3 == plVar1) {
      return;
    }
    if (((longlong *)plVar3[1] != plVar1) || (lVar2 = *plVar3, *(longlong **)(lVar2 + 8) != plVar3))
    break;
    *plVar1 = lVar2;
    *(longlong **)(lVar2 + 8) = plVar1;
    if (plVar3[0x43] != 0) {
      (*(code *)PTR__guard_dispatch_icall_140008188)();
      plVar3[0x43] = 0;
    }
    if (plVar3[0x44] != 0) {
      (*(code *)PTR__guard_dispatch_icall_140008188)();
      plVar3[0x44] = 0;
    }
    if (plVar3[0x46] != 0) {
      (*(code *)PTR__guard_dispatch_icall_140008188)();
      plVar3[0x46] = 0;
    }
    if (plVar3[0x45] != 0) {
      (*(code *)PTR__guard_dispatch_icall_140008188)();
      plVar3[0x45] = 0;
    }
    FUN_140007940(plVar3 + 2,0,(undefined1 *)0x208);
    FUN_1400048b8((longlong)plVar3);
  }
  pcVar4 = (code *)swi(0x29);
  (*pcVar4)(3);
  pcVar4 = (code *)swi(3);
  (*pcVar4)();
  return;
}



// === FUN_1400161c0 @ 1400161c0 (size=192) ===

uint FUN_1400161c0(longlong param_1,short *param_2,undefined8 *param_3,undefined8 *param_4)

{
  short sVar1;
  short sVar2;
  bool bVar3;
  short *psVar4;
  undefined8 *puVar5;
  
  puVar5 = *(undefined8 **)(param_1 + 0x68);
  bVar3 = false;
  do {
    if ((puVar5 == (undefined8 *)(param_1 + 0x68)) || (bVar3)) {
      return ~-(uint)bVar3 & 0xc0000034;
    }
    psVar4 = param_2;
    do {
      sVar1 = *psVar4;
      sVar2 = *(short *)((longlong)psVar4 + (longlong)puVar5 + (0x10 - (longlong)param_2));
      if (sVar1 != sVar2) break;
      psVar4 = psVar4 + 1;
    } while (sVar2 != 0);
    if (sVar1 == sVar2) {
      if (param_3 != (undefined8 *)0x0) {
        *param_3 = puVar5[0x43];
        (*(code *)PTR__guard_dispatch_icall_140008188)();
      }
      if (param_4 != (undefined8 *)0x0) {
        *param_4 = puVar5[0x44];
        (*(code *)PTR__guard_dispatch_icall_140008188)();
      }
      bVar3 = true;
    }
    puVar5 = (undefined8 *)*puVar5;
  } while( true );
}



// === FUN_140016280 @ 140016280 (size=5) ===

undefined8 FUN_140016280(longlong param_1)

{
  return *(undefined8 *)(param_1 + 0x30);
}



// === FUN_140016290 @ 140016290 (size=201) ===

uint FUN_140016290(longlong param_1,longlong param_2,undefined8 *param_3,undefined8 *param_4,
                  undefined8 *param_5,undefined8 *param_6)

{
  uint uVar1;
  undefined8 local_res18 [2];
  undefined8 local_38;
  undefined8 local_30;
  undefined8 local_28 [2];
  
  uVar1 = 0;
  local_res18[0] = 0;
  local_38 = 0;
  local_30 = 0;
  local_28[0] = 0;
  if ((param_3 != (undefined8 *)0x0) || (param_4 != (undefined8 *)0x0)) {
    uVar1 = FUN_1400161c0(param_1,*(short **)(param_2 + 8),local_res18,&local_38);
    if ((int)uVar1 < 0) {
      return uVar1;
    }
    if (param_3 != (undefined8 *)0x0) {
      *param_3 = local_res18[0];
    }
    if (param_4 != (undefined8 *)0x0) {
      *param_4 = local_38;
    }
  }
  if (((param_5 != (undefined8 *)0x0) || (param_6 != (undefined8 *)0x0)) &&
     (uVar1 = FUN_1400161c0(param_1,*(short **)(param_2 + 0x38),&local_30,local_28), -1 < (int)uVar1
     )) {
    if (param_5 != (undefined8 *)0x0) {
      *param_5 = local_30;
    }
    if (param_6 != (undefined8 *)0x0) {
      *param_6 = local_28[0];
    }
  }
  return uVar1;
}



// === FUN_140016360 @ 140016360 (size=5) ===

undefined8 FUN_140016360(longlong param_1)

{
  return *(undefined8 *)(param_1 + 0x38);
}



// === FUN_140016370 @ 140016370 (size=5) ===

undefined8 FUN_140016370(longlong param_1)

{
  return *(undefined8 *)(param_1 + 0x40);
}



// === FUN_140016380 @ 140016380 (size=1181) ===

/* WARNING: Function: __security_check_cookie replaced with injection: security_check_cookie */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

int FUN_140016380(longlong param_1,undefined8 param_2)

{
  longlong lVar1;
  int iVar2;
  uint uVar3;
  longlong *plVar4;
  undefined8 uVar5;
  uint uVar6;
  uint uVar7;
  uint uVar8;
  uint uVar9;
  undefined1 auStackY_198 [32];
  uint local_158;
  uint local_154;
  wchar_t local_148 [128];
  ulonglong local_48;
  
  local_48 = DAT_140012be0 ^ (ulonglong)auStackY_198;
  *(undefined8 *)(param_1 + 0x30) = param_2;
  *(undefined8 *)(param_1 + 0x50) = 0;
  *(undefined8 *)(param_1 + 0x28) = 0;
  *(undefined8 *)(param_1 + 0x58) = 0;
  *(undefined8 *)(param_1 + 0x38) = 0;
  local_154 = 0;
  *(undefined8 *)(param_1 + 0x40) = 0;
  lVar1 = param_1 + 0x68;
  *(undefined4 *)(param_1 + 0x48) = 1;
  *(longlong *)(param_1 + 0x70) = lVar1;
  *(longlong *)lVar1 = lVar1;
  iVar2 = PcGetPhysicalDeviceObject(param_2);
  if (iVar2 < 0) {
    return iVar2;
  }
  iVar2 = (*(code *)PTR__guard_dispatch_icall_140008188)(DAT_140013210,*DAT_140013210,0,param_2);
  if (-1 < iVar2) {
    plVar4 = (longlong *)FUN_1400048a8((undefined1 *)0x100,0x40);
    if (plVar4 == (longlong *)0x0) {
      *(undefined8 *)(param_1 + 0x50) = 0;
    }
    else {
      plVar4 = FUN_140017844(plVar4);
      *(longlong **)(param_1 + 0x50) = plVar4;
      if (plVar4 != (longlong *)0x0) {
        FUN_1400178ac(plVar4);
        local_158 = 0;
        uVar5 = FUN_14000436c(0,0x14001b2b0,0x14001b290,&local_158);
        uVar7 = 0x1c00;
        if (((((int)uVar5 == 0) ||
             (uVar5 = FUN_14000436c(0,0x14001b310,0x14001b290,&local_158), (int)uVar5 == 0)) &&
            (0x3ff < local_158)) && (uVar7 = local_158, 0x8000 < local_158)) {
          uVar7 = 0x8000;
        }
        local_158 = 0;
        uVar5 = FUN_14000436c(0,0x14001b2b0,0x14001b390,&local_158);
        uVar6 = uVar7;
        if ((((int)uVar5 == 0) ||
            (uVar5 = FUN_14000436c(0,0x14001b310,0x14001b390,&local_158), (int)uVar5 == 0)) &&
           ((0x2f < local_158 && (uVar6 = local_158, uVar7 < local_158)))) {
          uVar6 = uVar7;
        }
        local_158 = 0;
        uVar5 = FUN_14000436c(0,0x14001b2b0,0x14001b3d0,&local_158);
        uVar9 = 96000;
        if (((((int)uVar5 == 0) ||
             (uVar5 = FUN_14000436c(0,0x14001b310,0x14001b3d0,&local_158), (int)uVar5 == 0)) &&
            ((32000 < local_158 &&
             (((uVar9 = local_158, local_158 != 0xac44 && (local_158 != 48000)) &&
              (local_158 != 0x15888)))))) && ((local_158 != 0x2b110 && (local_158 != 0x2ee00)))) {
          uVar9 = 96000;
        }
        local_158 = 0;
        uVar5 = FUN_14000436c(0,0x14001b2b0,0x14001b400,&local_158);
        if (((int)uVar5 == 0) ||
           (uVar5 = FUN_14000436c(0,0x14001b310,0x14001b400,&local_158), (int)uVar5 == 0)) {
          local_154 = local_158;
        }
        local_158 = 0;
        uVar5 = FUN_14000436c(0,0x14001b2b0,0x14001b430,&local_158);
        if (((int)uVar5 == 0) ||
           (uVar5 = FUN_14000436c(0,0x14001b310,0x14001b430,&local_158), uVar8 = 0, (int)uVar5 == 0)
           ) {
          uVar8 = local_158;
        }
        DAT_140012e60 = (uint *)FUN_140001000(uVar7 + 8,0x10,uVar9);
        if (DAT_140012e60 == (uint *)0x0) {
          iVar2 = -0x3fffff66;
        }
        if (uVar8 == 0) {
          DAT_140012e68 = (uint *)0x0;
        }
        else {
          DAT_140012e68 = (uint *)FUN_140001000(uVar7 + 8,0x10,uVar9);
          if (DAT_140012e68 == (uint *)0x0) {
            iVar2 = -0x3fffff66;
          }
        }
        local_158 = 0;
        swprintf(local_148,0x14001b470,L"VBAudioCableAWDM_Latency",1);
        uVar5 = FUN_14000436c(0,0x14001b2b0,(longlong)local_148,&local_158);
        if (((((int)uVar5 == 0) ||
             (uVar5 = FUN_14000436c(0,0x14001b310,(longlong)local_148,&local_158), (int)uVar5 == 0))
            && (0x2f < local_158)) && (uVar6 = local_158, uVar7 < local_158)) {
          uVar6 = uVar7;
        }
        if (DAT_140012e60 != (uint *)0x0) {
          *DAT_140012e60 = uVar6;
        }
        if (DAT_140012e68 != (uint *)0x0) {
          *DAT_140012e68 = uVar6;
          DAT_140012e68[99] = 0x67;
        }
        _DAT_140012f48 = 0;
        _DAT_140012f38 = 0;
        _DAT_140012f50 = 0;
        _DAT_140012f58 = 0;
        _DAT_140012f68 = 0;
        _DAT_140012f60 = 0;
        _DAT_140012ef0 = 0;
        uRam0000000140012ef8 = 0;
        _DAT_140012f00 = 0;
        uRam0000000140012f08 = 0;
        _DAT_140012f10 = 0;
        uRam0000000140012f18 = 0;
        _DAT_140012f20 = 0;
        uRam0000000140012f28 = 0;
        DAT_140012f30 = FUN_140003a04(0x40,(undefined1 *)0x20000);
        if (DAT_140012f30 == (longlong *)0x0) {
          iVar2 = -0x3fffff66;
        }
        DAT_140012f40 = FUN_140003a04(0x40,(undefined1 *)0x20000);
        if (DAT_140012f40 == (longlong *)0x0) {
          iVar2 = -0x3fffff66;
        }
        uVar3 = 0;
        local_158 = 0;
        if (local_154 != 0) {
          uVar3 = 2;
          local_158 = 2;
        }
        if (uVar8 != 0) {
          uVar3 = uVar3 | 4;
          local_158 = uVar3;
        }
        FUN_1400040b0(0,uVar9,uVar7,uVar6,0,uVar3);
        return iVar2;
      }
    }
    return -0x3fffff66;
  }
  return iVar2;
}



// === FUN_140016820 @ 140016820 (size=1026) ===

uint FUN_140016820(longlong *param_1,undefined4 param_2,undefined8 param_3,longlong param_4,
                  undefined8 param_5,undefined8 *param_6,undefined8 *param_7,undefined8 *param_8,
                  undefined8 *param_9)

{
  bool bVar1;
  bool bVar2;
  uint uVar3;
  int iVar4;
  longlong *local_60;
  longlong *local_58;
  longlong *local_50;
  longlong *local_48 [2];
  
  local_60 = (longlong *)0x0;
  local_50 = (longlong *)0x0;
  bVar1 = false;
  bVar2 = false;
  local_58 = (longlong *)0x0;
  local_48[0] = (longlong *)0x0;
  if (param_6 != (undefined8 *)0x0) {
    *param_6 = 0;
  }
  if (param_7 != (undefined8 *)0x0) {
    *param_7 = 0;
  }
  if (param_8 != (undefined8 *)0x0) {
    *param_8 = 0;
  }
  if (param_9 != (undefined8 *)0x0) {
    *param_9 = 0;
  }
  uVar3 = FUN_1400161c0((longlong)param_1,*(short **)(param_4 + 8),&local_60,&local_58);
  if ((((int)uVar3 < 0) || (local_60 == (longlong *)0x0)) || (local_58 == (longlong *)0x0)) {
    bVar1 = true;
    iVar4 = (*(code *)PTR__guard_dispatch_icall_140008188)
                      (param_1,param_2,param_3,*(undefined8 *)(param_4 + 8),
                       *(undefined8 *)(param_4 + 0x10),&DAT_140008a20,&DAT_140008a20,
                       *(undefined8 *)(param_4 + 0x18),*(undefined4 *)(param_4 + 0x28),
                       *(undefined8 *)(param_4 + 0x30),param_5,param_4,0,&DAT_140008910,0,&local_60,
                       &local_58);
    if (-1 < iVar4) {
      FUN_140015790((longlong)param_1,*(longlong *)(param_4 + 8),local_60,local_58);
    }
  }
  uVar3 = FUN_1400161c0((longlong)param_1,*(short **)(param_4 + 0x38),&local_50,local_48);
  if ((((int)uVar3 < 0) || (local_50 == (longlong *)0x0)) || (local_48[0] == (longlong *)0x0)) {
    bVar2 = true;
    uVar3 = (*(code *)PTR__guard_dispatch_icall_140008188)
                      (param_1,param_2,param_3,*(undefined8 *)(param_4 + 0x38),
                       *(undefined8 *)(param_4 + 0x40),&DAT_140008a30,&DAT_140008a30,
                       *(undefined8 *)(param_4 + 0x48),*(undefined4 *)(param_4 + 0x58),
                       *(undefined8 *)(param_4 + 0x60),param_5,param_4,0,&DAT_140008930,0,&local_50,
                       local_48);
    if (-1 < (int)uVar3) {
      uVar3 = FUN_140015790((longlong)param_1,*(longlong *)(param_4 + 0x38),local_50,local_48[0]);
    }
  }
  if ((local_60 != (longlong *)0x0) && (local_50 != (longlong *)0x0)) {
    uVar3 = (*(code *)PTR__guard_dispatch_icall_140008188)
                      (param_1,local_60,local_50,*(undefined8 *)(param_4 + 0x80),
                       *(undefined4 *)(param_4 + 0x88));
  }
  if ((int)uVar3 < 0) {
    if ((bVar1) && (local_60 != (longlong *)0x0)) {
      (*(code *)PTR__guard_dispatch_icall_140008188)(param_1);
      FUN_1400173c0((longlong)param_1,*(short **)(param_4 + 8));
    }
    if ((bVar2) && (local_50 != (longlong *)0x0)) {
      (*(code *)PTR__guard_dispatch_icall_140008188)(param_1,local_50);
      FUN_1400173c0((longlong)param_1,*(short **)(param_4 + 0x38));
    }
  }
  else {
    if ((param_6 != (undefined8 *)0x0) && (local_60 != (longlong *)0x0)) {
      (*(code *)PTR__guard_dispatch_icall_140008188)(local_60);
      *param_6 = local_60;
    }
    if ((param_7 != (undefined8 *)0x0) && (local_50 != (longlong *)0x0)) {
      (*(code *)PTR__guard_dispatch_icall_140008188)(local_50);
      *param_7 = local_50;
    }
    if ((param_8 != (undefined8 *)0x0) && (local_58 != (longlong *)0x0)) {
      (*(code *)PTR__guard_dispatch_icall_140008188)();
      *param_8 = local_58;
    }
    if ((param_9 != (undefined8 *)0x0) && (local_48[0] != (longlong *)0x0)) {
      (*(code *)PTR__guard_dispatch_icall_140008188)(local_48[0]);
      *param_9 = local_48[0];
    }
  }
  if (local_58 != (longlong *)0x0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)();
    local_58 = (longlong *)0x0;
  }
  if (local_60 != (longlong *)0x0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)(local_60);
    local_60 = (longlong *)0x0;
  }
  if (local_48[0] != (longlong *)0x0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)(local_48[0]);
    local_48[0] = (longlong *)0x0;
  }
  if (local_50 != (longlong *)0x0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)(local_50);
  }
  return uVar3;
}



// === FUN_140016c30 @ 140016c30 (size=480) ===

/* WARNING: Type propagation algorithm not settling */

ulonglong FUN_140016c30(longlong *param_1,undefined4 param_2,undefined8 param_3,undefined8 param_4,
                       longlong param_5,undefined8 param_6,undefined8 param_7,undefined *param_8,
                       uint param_9,longlong param_10,undefined8 param_11,undefined8 param_12,
                       undefined8 param_13,undefined8 param_14,longlong param_15,longlong param_16,
                       longlong param_17)

{
  uint uVar1;
  ulonglong uVar2;
  ulonglong uVar3;
  longlong local_res8;
  longlong local_38 [4];
  
  local_res8 = 0;
  local_38[0] = 0;
  local_38[1] = 0;
  local_38[2] = 0;
  uVar2 = FUN_140015f14(param_1,param_4,param_5,param_9,param_10,local_38 + 1);
  uVar3 = uVar2 & 0xffffffff;
  if (-1 < (int)uVar2) {
    RtlFreeUnicodeString(local_38 + 1);
    uVar1 = PcNewPort(&local_res8,param_6);
    uVar3 = (ulonglong)uVar1;
    if (-1 < (int)uVar1) {
      if (param_8 == (undefined *)0x0) {
        uVar1 = PcNewMiniport(local_38,param_7);
      }
      else {
        uVar1 = (*(code *)PTR__guard_dispatch_icall_140008188)
                          (param_2,local_38,param_7,0,0x40,param_1,param_11,param_12);
      }
      uVar3 = (ulonglong)uVar1;
      if (-1 < (int)uVar1) {
        uVar1 = (*(code *)PTR__guard_dispatch_icall_140008188)
                          (local_res8,param_1[6],param_3,local_38[0],param_1,param_13);
        uVar3 = (ulonglong)uVar1;
        if (-1 < (int)uVar1) {
          uVar1 = PcRegisterSubdevice(param_1[6],param_4,local_res8);
          uVar3 = (ulonglong)uVar1;
          if (-1 < (int)uVar1) {
            if (param_16 != 0) {
              uVar1 = (*(code *)PTR__guard_dispatch_icall_140008188)(local_res8,&DAT_1400088e0);
              uVar3 = (ulonglong)uVar1;
            }
            if (param_15 != 0) {
              uVar1 = (*(code *)PTR__guard_dispatch_icall_140008188)(local_res8,param_14);
              uVar3 = (ulonglong)uVar1;
            }
            if (param_17 != 0) {
              uVar1 = (*(code *)PTR__guard_dispatch_icall_140008188)(local_38[0],&DAT_1400088e0);
              uVar3 = (ulonglong)uVar1;
            }
          }
        }
      }
    }
  }
  if (local_res8 != 0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)();
  }
  if (local_38[0] != 0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)();
  }
  return uVar3;
}



// === FUN_140016e10 @ 140016e10 (size=933) ===

int FUN_140016e10(longlong *param_1,undefined8 param_2,undefined8 param_3)

{
  short sVar1;
  longlong lVar2;
  longlong lVar3;
  int iVar4;
  undefined8 uVar5;
  longlong *plVar6;
  longlong *plVar7;
  longlong *plVar8;
  longlong *plVar9;
  ulonglong uVar10;
  longlong lVar11;
  ulonglong uVar12;
  longlong *plVar13;
  uint uVar14;
  uint local_res8 [2];
  undefined4 local_res20 [2];
  uint *puVar15;
  longlong local_e8;
  longlong local_e0;
  longlong local_d8;
  longlong local_d0;
  undefined1 local_c8 [16];
  undefined1 local_b8 [16];
  undefined1 local_a8 [16];
  undefined4 local_98;
  longlong local_90;
  undefined1 *local_88;
  undefined4 local_80;
  undefined8 local_78;
  undefined8 uStack_70;
  undefined4 local_68 [2];
  longlong local_60;
  undefined1 *local_58;
  undefined4 local_50;
  undefined8 local_48;
  undefined8 uStack_40;
  
  plVar7 = (longlong *)0x0;
  local_d0 = 0;
  local_d8 = 0;
  RtlInitUnicodeString(local_c8,0);
  RtlInitUnicodeString(local_a8,param_3);
  uVar5 = (*(code *)PTR__guard_dispatch_icall_140008188)(param_1);
  IoRegisterDeviceInterface(uVar5,&DAT_140008430,local_a8,local_c8);
  iVar4 = IoOpenDeviceInterfaceRegistryKey(local_c8,0x80000000,&local_d8);
  if ((-1 < iVar4) &&
     (iVar4 = IoOpenDeviceInterfaceRegistryKey(param_2,0x40000000,&local_d0), lVar3 = local_d0,
     lVar2 = local_d8, -1 < iVar4)) {
    local_res8[0] = 0;
    local_res20[0] = 0;
    local_e8 = 0;
    local_e0 = 0;
    if ((local_d8 == 0) || (local_d0 == 0)) {
      iVar4 = -0x3ffffff3;
    }
    else {
      uVar14 = 0x118;
      plVar6 = FUN_140003a04(0x40,(undefined1 *)0x118);
      plVar13 = plVar7;
      if (plVar6 == (longlong *)0x0) {
        iVar4 = -0x3fffff66;
      }
      else {
        while( true ) {
          puVar15 = local_res8;
          iVar4 = ZwEnumerateKey(lVar2,plVar13,0,plVar6,uVar14,puVar15);
          if (iVar4 == -0x7fffffe6) break;
          if ((iVar4 == -0x3fffffdd) || (iVar4 == -0x7ffffffb)) {
            FUN_140003a54((longlong)plVar6);
            uVar14 = local_res8[0];
            plVar6 = FUN_140003a04(0x40,(undefined1 *)(ulonglong)local_res8[0]);
            if (plVar6 != (longlong *)0x0) {
              puVar15 = local_res8;
              iVar4 = ZwEnumerateKey(lVar2,plVar13,0,plVar6,uVar14,puVar15);
              if (iVar4 != -0x7fffffe6) goto LAB_140016f51;
              break;
            }
            iVar4 = -0x3fffff66;
          }
          else {
LAB_140016f51:
            if (-1 < iVar4) {
              plVar7 = FUN_140003a04(0x40,(undefined1 *)
                                          ((ulonglong)*(uint *)((longlong)plVar6 + 0xc) + 2));
              uVar12 = (ulonglong)*(uint *)((longlong)plVar6 + 0xc) + 2 >> 1;
              if (uVar12 - 1 < 0x7fffffff) {
                uVar10 = (ulonglong)(*(uint *)((longlong)plVar6 + 0xc) >> 1);
                if (uVar10 < 0x7fffffff) {
                  lVar11 = uVar10 - uVar12;
                  plVar9 = plVar7;
                  do {
                    if ((lVar11 + uVar12 == 0) ||
                       (sVar1 = *(short *)((longlong)plVar6 + (0x10 - (longlong)plVar7) +
                                          (longlong)plVar9), sVar1 == 0)) break;
                    *(short *)plVar9 = sVar1;
                    plVar9 = (longlong *)((longlong)plVar9 + 2);
                    uVar12 = uVar12 - 1;
                  } while (uVar12 != 0);
                  plVar8 = (longlong *)((longlong)plVar9 + -2);
                  if (uVar12 != 0) {
                    plVar8 = plVar9;
                  }
                  *(short *)plVar8 = 0;
                }
                else {
                  *(undefined2 *)plVar7 = 0;
                }
              }
              *(undefined2 *)
               ((longlong)plVar7 + (ulonglong)(*(uint *)((longlong)plVar6 + 0xc) >> 1) * 2) = 0;
              RtlInitUnicodeString(local_b8,plVar7);
              local_88 = local_b8;
              local_98 = 0x30;
              local_90 = lVar2;
              local_80 = 0x240;
              local_78 = 0;
              uStack_70 = 0;
              iVar4 = ZwOpenKey(&local_e8,0x20019);
              if (-1 < iVar4) {
                local_58 = local_b8;
                local_68[0] = 0x30;
                local_60 = lVar3;
                local_50 = 0x200;
                local_48 = 0;
                uStack_40 = 0;
                iVar4 = ZwCreateKey(&local_e0,0x20006,local_68,0,0,
                                    (ulonglong)puVar15 & 0xffffffff00000000,local_res20);
                if (-1 < iVar4) {
                  iVar4 = FUN_1400159e4(local_e8,local_e0,1);
                  goto LAB_140017173;
                }
              }
              ZwClose(local_e8);
            }
          }
LAB_140017173:
          if (plVar7 != (longlong *)0x0) {
            FUN_140003a54((longlong)plVar7);
          }
          if (local_e8 != 0) {
            ZwClose();
          }
          if (local_e0 != 0) {
            ZwClose();
          }
          if (iVar4 < 0) goto LAB_14001700d;
          plVar13 = (longlong *)(ulonglong)((int)plVar13 + 1);
        }
        iVar4 = 0;
LAB_14001700d:
        if (plVar6 != (longlong *)0x0) {
          FUN_140003a54((longlong)plVar6);
        }
      }
    }
  }
  RtlFreeUnicodeString(local_c8);
  if (local_d8 != 0) {
    ZwClose();
  }
  if (local_d0 != 0) {
    ZwClose();
  }
  return iVar4;
}



// === FUN_1400171c0 @ 1400171c0 (size=23) ===

void FUN_1400171c0(longlong param_1)

{
  if (*(longlong **)(param_1 + 0x50) != (longlong *)0x0) {
    FUN_1400178ac(*(longlong **)(param_1 + 0x50));
  }
  return;
}



// === FUN_1400171d8 @ 1400171d8 (size=183) ===

undefined8
FUN_1400171d8(undefined8 *param_1,undefined8 param_2,undefined8 *param_3,undefined8 param_4)

{
  undefined8 *puVar1;
  undefined8 uVar2;
  bool bVar3;
  
  LOCK();
  bVar3 = DAT_140012f70 == 0;
  if (bVar3) {
    DAT_140012f70 = 1;
  }
  UNLOCK();
  if (bVar3) {
    puVar1 = (undefined8 *)FUN_1400048a8((undefined1 *)0x78,param_4);
    uVar2 = 0;
    if (puVar1 == (undefined8 *)0x0) {
      uVar2 = 0xc000009a;
    }
    else {
      FUN_140006e00(puVar1 + 2,param_3);
      *puVar1 = &PTR_FUN_140008c90;
      puVar1[1] = &PTR_FUN_140008da0;
      puVar1[2] = &PTR_FUN_140008dd0;
      *param_1 = puVar1;
      (*(code *)PTR__guard_dispatch_icall_140008188)(puVar1);
    }
  }
  else {
    uVar2 = 0x80000011;
  }
  return uVar2;
}



// === FUN_140017290 @ 140017290 (size=135) ===

undefined8 FUN_140017290(longlong param_1,longlong *param_2,undefined8 *param_3)

{
  longlong lVar1;
  ulonglong uVar2;
  
  lVar1 = *param_2;
  if (((lVar1 == DAT_1400088e0) && (param_2[1] == DAT_1400088e8)) ||
     ((lVar1 == DAT_140008a40 && (param_2[1] == DAT_140008a48)))) {
    uVar2 = param_1 - 0x10;
  }
  else {
    if ((lVar1 != DAT_1400089d0) || (param_2[1] != DAT_1400089d8)) {
      *param_3 = 0;
      return 0xc000000d;
    }
    uVar2 = -(ulonglong)(param_1 != 0x10) & param_1 - 8U;
  }
  *param_3 = uVar2;
  if (uVar2 == 0) {
    return 0xc000000d;
  }
  (*(code *)PTR__guard_dispatch_icall_140008188)();
  return 0;
}



// === FUN_140017320 @ 140017320 (size=158) ===

undefined8
FUN_140017320(longlong param_1,undefined8 param_2,undefined4 param_3,undefined4 param_4,
             undefined8 param_5,undefined4 param_6,undefined4 param_7)

{
  undefined8 *puVar1;
  
  for (puVar1 = *(undefined8 **)(param_1 + 0x68); puVar1 != (undefined8 *)(param_1 + 0x68);
      puVar1 = (undefined8 *)*puVar1) {
    if (puVar1[0x46] != 0) {
      (*(code *)PTR__guard_dispatch_icall_140008188)
                (puVar1[0x46],param_2,param_3,param_4,param_5,param_6,param_7);
    }
  }
  return 0;
}



// === FUN_1400173c0 @ 1400173c0 (size=301) ===

ulonglong FUN_1400173c0(longlong param_1,short *param_2)

{
  short sVar1;
  short sVar2;
  longlong lVar3;
  longlong *plVar4;
  code *pcVar5;
  byte bVar6;
  short *psVar7;
  ulonglong uVar8;
  longlong *plVar9;
  
  bVar6 = 0;
  plVar9 = *(longlong **)(param_1 + 0x68);
  do {
    if (plVar9 == (longlong *)(param_1 + 0x68)) {
LAB_140017409:
      return (ulonglong)(~-(uint)bVar6 & 0xc0000034);
    }
    psVar7 = param_2;
    do {
      sVar1 = *psVar7;
      sVar2 = *(short *)((longlong)psVar7 + (longlong)plVar9 + (0x10 - (longlong)param_2));
      if (sVar1 != sVar2) break;
      psVar7 = psVar7 + 1;
    } while (sVar2 != 0);
    if (sVar1 == sVar2) {
      if (plVar9[0x43] != 0) {
        (*(code *)PTR__guard_dispatch_icall_140008188)();
        plVar9[0x43] = 0;
      }
      if (plVar9[0x44] != 0) {
        (*(code *)PTR__guard_dispatch_icall_140008188)();
        plVar9[0x44] = 0;
      }
      if (plVar9[0x45] != 0) {
        (*(code *)PTR__guard_dispatch_icall_140008188)();
        plVar9[0x45] = 0;
      }
      if (plVar9[0x46] != 0) {
        (*(code *)PTR__guard_dispatch_icall_140008188)();
        plVar9[0x46] = 0;
      }
      FUN_140007940(plVar9 + 2,0,(undefined1 *)0x208);
      lVar3 = *plVar9;
      if ((*(longlong **)(lVar3 + 8) != plVar9) ||
         (plVar4 = (longlong *)plVar9[1], (longlong *)*plVar4 != plVar9)) {
        pcVar5 = (code *)swi(0x29);
        (*pcVar5)(3);
        pcVar5 = (code *)swi(3);
        uVar8 = (*pcVar5)();
        return uVar8;
      }
      *plVar4 = lVar3;
      *(longlong **)(lVar3 + 8) = plVar4;
      bVar6 = 1;
      FUN_1400048b8((longlong)plVar9);
      goto LAB_140017409;
    }
    plVar9 = (longlong *)*plVar9;
  } while( true );
}



// === FUN_1400174f0 @ 1400174f0 (size=173) ===

undefined8 FUN_1400174f0(longlong *param_1,longlong param_2,longlong param_3,longlong param_4)

{
  if ((param_3 != 0) && (param_4 != 0)) {
    (*(code *)PTR__guard_dispatch_icall_140008188)
              (param_1,param_3,param_4,*(undefined8 *)(param_2 + 0x80),
               *(undefined4 *)(param_2 + 0x88));
  }
  FUN_1400173c0((longlong)param_1,*(short **)(param_2 + 0x38));
  (*(code *)PTR__guard_dispatch_icall_140008188)(param_1,param_4);
  FUN_1400173c0((longlong)param_1,*(short **)(param_2 + 8));
  (*(code *)PTR__guard_dispatch_icall_140008188)(param_1,param_3);
  return 0;
}



// === FUN_1400175a0 @ 1400175a0 (size=74) ===

void FUN_1400175a0(longlong param_1,longlong *param_2)

{
  if (*(longlong *)(param_1 + 0x58) != 0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)();
  }
  *(longlong **)(param_1 + 0x58) = param_2;
  if (param_2 != (longlong *)0x0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)(param_2);
  }
  return;
}



// === FUN_1400175f0 @ 1400175f0 (size=255) ===

int FUN_1400175f0(longlong *param_1,undefined8 param_2,int param_3)

{
  int iVar1;
  int iVar2;
  longlong local_res8;
  longlong local_res20;
  
  local_res8 = 0;
  local_res20 = 0;
  iVar1 = (*(code *)PTR__guard_dispatch_icall_140008188)(param_1,param_2,0,0,&local_res8,0);
  if (-1 < iVar1) {
    iVar1 = (*(code *)PTR__guard_dispatch_icall_140008188)(local_res8,&DAT_140008970,&local_res20);
    if (-1 < iVar1) {
      if (param_3 == 0) {
        if ((int)param_1[0xc] == 0) {
          (*(code *)PTR__guard_dispatch_icall_140008188)(local_res20,param_1[6],0);
        }
        *(int *)(param_1 + 0xc) = (int)param_1[0xc] + 1;
      }
      else {
        iVar2 = (int)param_1[0xc] + -1;
        *(int *)(param_1 + 0xc) = iVar2;
        if (iVar2 == 0) {
          (*(code *)PTR__guard_dispatch_icall_140008188)(local_res20,param_1[6],1);
        }
      }
    }
  }
  if (local_res8 != 0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)();
    local_res8 = 0;
  }
  if (local_res20 != 0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)();
  }
  return iVar1;
}



// === FUN_1400176f0 @ 1400176f0 (size=74) ===

void FUN_1400176f0(longlong param_1,longlong *param_2)

{
  if (*(longlong *)(param_1 + 0x28) != 0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)();
  }
  *(longlong **)(param_1 + 0x28) = param_2;
  if (param_2 != (longlong *)0x0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)(param_2);
  }
  return;
}



// === FUN_14001773c @ 14001773c (size=121) ===

undefined8 FUN_14001773c(undefined8 param_1,uint param_2,longlong param_3)

{
  undefined8 uVar1;
  undefined4 *puVar2;
  uint uVar3;
  
  if ((param_3 != 0) && (uVar3 = 0, param_2 != 0)) {
    puVar2 = (undefined4 *)(param_3 + 0xc);
    do {
      uVar1 = IoSetDeviceInterfacePropertyData
                        (param_1,*(undefined8 *)(puVar2 + -3),0,1,puVar2[-1],*puVar2,
                         *(undefined8 *)(puVar2 + 1));
      if ((int)uVar1 < 0) {
        return uVar1;
      }
      uVar3 = uVar3 + 1;
      puVar2 = puVar2 + 6;
    } while (uVar3 < param_2);
  }
  return 0;
}



// === FUN_1400177c0 @ 1400177c0 (size=132) ===

int FUN_1400177c0(longlong param_1,undefined8 *param_2)

{
  int iVar1;
  undefined8 local_res10;
  
  local_res10 = 0;
  if (param_2 == (undefined8 *)0x0) {
    iVar1 = 0;
  }
  else {
    iVar1 = (*(code *)PTR__guard_dispatch_icall_140008188)(param_2,&DAT_140008a00,&local_res10);
    if (-1 < iVar1) {
      iVar1 = (*(code *)PTR__guard_dispatch_icall_140008188)
                        (local_res10,*(undefined8 *)(param_1 + 0x30),param_2);
      (*(code *)PTR__guard_dispatch_icall_140008188)();
    }
  }
  return iVar1;
}



// === FUN_140017844 @ 140017844 (size=104) ===

longlong * FUN_140017844(longlong *param_1)

{
  longlong lVar1;
  longlong *plVar2;
  
  *(undefined8 *)((longlong)param_1 + 0xf4) = 0;
  *(undefined4 *)((longlong)param_1 + 0xfc) = 0;
  FUN_140007940(param_1 + 10,0xff,(undefined1 *)0x50);
  FUN_140007940(param_1,0,(undefined1 *)0x50);
  plVar2 = param_1 + 0x14;
  for (lVar1 = 0x14; lVar1 != 0; lVar1 = lVar1 + -1) {
    *(undefined4 *)plVar2 = 0x3fffffff;
    plVar2 = (longlong *)((longlong)plVar2 + 4);
  }
  *(undefined4 *)(param_1 + 0x1e) = 2;
  return param_1;
}



// === FUN_1400178ac @ 1400178ac (size=88) ===

void FUN_1400178ac(longlong *param_1)

{
  longlong lVar1;
  longlong *plVar2;
  
  FUN_140007940(param_1 + 10,0xff,(undefined1 *)0x50);
  FUN_140007940(param_1,0,(undefined1 *)0x50);
  plVar2 = param_1 + 0x14;
  for (lVar1 = 0x14; lVar1 != 0; lVar1 = lVar1 + -1) {
    *(undefined4 *)plVar2 = 0x3fffffff;
    plVar2 = (longlong *)((longlong)plVar2 + 4);
  }
  *(undefined4 *)(param_1 + 0x1e) = 2;
  return;
}



// === FUN_140017904 @ 140017904 (size=107) ===

longlong FUN_140017904(longlong param_1)

{
  longlong lVar1;
  longlong lVar2;
  
  lVar2 = 0;
  if (((param_1 != 0) && (*(longlong *)(param_1 + 0x10) == DAT_140008df0)) &&
     (*(longlong *)(param_1 + 0x18) == DAT_140008df8)) {
    lVar1 = *(longlong *)(param_1 + 0x30);
    if ((((lVar1 == DAT_140008e00) && (*(longlong *)(param_1 + 0x38) == DAT_140008e08)) ||
        ((lVar2 = 0, lVar1 == DAT_140008e10 && (*(longlong *)(param_1 + 0x38) == DAT_140008e18))))
       && ((lVar2 = param_1 + 0x40, lVar1 == DAT_140008e10 &&
           (*(longlong *)(param_1 + 0x38) == DAT_140008e18)))) {
      lVar2 = param_1 + 0x48;
    }
  }
  return lVar2;
}



// === FUN_140017970 @ 140017970 (size=110) ===

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined8 FUN_140017970(longlong param_1,undefined4 param_2,int param_3)

{
  undefined4 *puVar1;
  int iVar2;
  undefined4 uVar3;
  undefined4 uVar4;
  undefined4 uVar5;
  undefined4 uVar6;
  
  if (0x27 < *(uint *)(param_1 + 0x30)) {
    puVar1 = *(undefined4 **)(param_1 + 0x38);
    iVar2 = 0;
    *puVar1 = param_2;
    puVar1[1] = 0x28;
    uVar3 = _DAT_140008e30;
    uVar4 = _UNK_140008e34;
    uVar5 = _UNK_140008e38;
    uVar6 = _UNK_140008e3c;
    if (param_3 != 0xffff) {
      uVar3 = _DAT_140008e20;
      uVar4 = _UNK_140008e24;
      uVar5 = _UNK_140008e28;
      uVar6 = _UNK_140008e2c;
      iVar2 = param_3;
    }
    puVar1[2] = uVar3;
    puVar1[3] = uVar4;
    puVar1[4] = uVar5;
    puVar1[5] = uVar6;
    puVar1[6] = iVar2;
    *(undefined8 *)(puVar1 + 7) = 0;
    puVar1[9] = 0;
    *(undefined4 *)(param_1 + 0x30) = 0x28;
    return 0;
  }
  if (*(uint *)(param_1 + 0x30) < 4) {
    *(undefined4 *)(param_1 + 0x30) = 0;
    return 0xc0000023;
  }
  **(undefined4 **)(param_1 + 0x38) = param_2;
  *(undefined4 *)(param_1 + 0x30) = 4;
  return 0;
}



// === FUN_1400179e0 @ 1400179e0 (size=176) ===

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined8 FUN_1400179e0(longlong param_1,uint param_2)

{
  undefined8 uVar1;
  undefined8 uVar2;
  undefined4 *puVar3;
  undefined8 uVar4;
  uint uVar5;
  ulonglong uVar6;
  
  uVar4 = 0;
  uVar5 = param_2 * 0x10 + 0x38;
  if (*(uint *)(param_1 + 0x30) < 0x28) {
    if (*(uint *)(param_1 + 0x30) < 4) {
      *(undefined4 *)(param_1 + 0x30) = 0;
      uVar4 = 0xc0000023;
    }
    else {
      *(undefined4 *)(param_1 + 0x30) = 4;
      **(undefined4 **)(param_1 + 0x38) = 0x203;
    }
  }
  else {
    puVar3 = *(undefined4 **)(param_1 + 0x38);
    *puVar3 = 0x203;
    puVar3[1] = uVar5;
    uVar2 = _UNK_140008e28;
    uVar1 = _DAT_140008e20;
    *(undefined8 *)(puVar3 + 6) = 0xb;
    *(undefined8 *)(puVar3 + 8) = 1;
    *(undefined8 *)(puVar3 + 2) = uVar1;
    *(undefined8 *)(puVar3 + 4) = uVar2;
    if (*(uint *)(param_1 + 0x30) < uVar5) {
      uVar5 = 0x28;
    }
    else {
      puVar3[0xb] = 0x10;
      puVar3[10] = 2;
      puVar3[0xd] = 2;
      puVar3[0xc] = param_2;
      if (param_2 != 0) {
        puVar3 = puVar3 + 0x10;
        uVar6 = (ulonglong)param_2;
        do {
          puVar3[1] = 1;
          *(undefined8 *)(puVar3 + -1) = 0;
          puVar3[-2] = 1;
          puVar3 = puVar3 + 4;
          uVar6 = uVar6 - 1;
        } while (uVar6 != 0);
      }
    }
    *(uint *)(param_1 + 0x30) = uVar5;
  }
  return uVar4;
}



// === FUN_140017a90 @ 140017a90 (size=186) ===

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined4 FUN_140017a90(longlong param_1,uint param_2)

{
  undefined8 uVar1;
  undefined8 uVar2;
  undefined4 *puVar3;
  undefined4 uVar4;
  uint uVar5;
  ulonglong uVar6;
  
  uVar4 = 0;
  uVar5 = param_2 * 0x10 + 0x38;
  if (*(uint *)(param_1 + 0x30) < 0x28) {
    if (*(uint *)(param_1 + 0x30) < 4) {
      *(undefined4 *)(param_1 + 0x30) = 0;
      uVar4 = 0xc0000023;
    }
    else {
      *(undefined4 *)(param_1 + 0x30) = 4;
      **(undefined4 **)(param_1 + 0x38) = 0x203;
    }
  }
  else {
    puVar3 = *(undefined4 **)(param_1 + 0x38);
    *puVar3 = 0x201;
    puVar3[1] = uVar5;
    uVar2 = _UNK_140008e28;
    uVar1 = _DAT_140008e20;
    *(undefined8 *)(puVar3 + 6) = 3;
    *(undefined8 *)(puVar3 + 8) = 1;
    *(undefined8 *)(puVar3 + 2) = uVar1;
    *(undefined8 *)(puVar3 + 4) = uVar2;
    if (*(uint *)(param_1 + 0x30) < uVar5) {
      uVar5 = 0x28;
    }
    else {
      puVar3[0xb] = 0x10;
      puVar3[10] = 2;
      puVar3[0xd] = 2;
      puVar3[0xc] = param_2;
      if (param_2 != 0) {
        puVar3 = puVar3 + 0x10;
        uVar6 = (ulonglong)param_2;
        do {
          puVar3[1] = 0x7fffffff;
          *puVar3 = 0x80000000;
          *(undefined8 *)(puVar3 + -2) = 0x1000;
          puVar3 = puVar3 + 4;
          uVar6 = uVar6 - 1;
        } while (uVar6 != 0);
      }
    }
    *(uint *)(param_1 + 0x30) = uVar5;
    uVar4 = 0;
  }
  return uVar4;
}



// === FUN_140017b4c @ 140017b4c (size=183) ===

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined4 FUN_140017b4c(longlong param_1,uint param_2)

{
  undefined8 uVar1;
  undefined8 uVar2;
  undefined4 *puVar3;
  undefined4 uVar4;
  uint uVar5;
  ulonglong uVar6;
  
  uVar4 = 0;
  uVar5 = param_2 * 0x10 + 0x38;
  if (*(uint *)(param_1 + 0x30) < 0x28) {
    if (*(uint *)(param_1 + 0x30) < 4) {
      *(undefined4 *)(param_1 + 0x30) = 0;
      uVar4 = 0xc0000023;
    }
    else {
      *(undefined4 *)(param_1 + 0x30) = 4;
      **(undefined4 **)(param_1 + 0x38) = 0x203;
    }
  }
  else {
    puVar3 = *(undefined4 **)(param_1 + 0x38);
    *puVar3 = 0x203;
    puVar3[1] = uVar5;
    uVar2 = _UNK_140008e28;
    uVar1 = _DAT_140008e20;
    *(undefined8 *)(puVar3 + 6) = 3;
    *(undefined8 *)(puVar3 + 8) = 1;
    *(undefined8 *)(puVar3 + 2) = uVar1;
    *(undefined8 *)(puVar3 + 4) = uVar2;
    if (*(uint *)(param_1 + 0x30) < uVar5) {
      uVar5 = 0x28;
    }
    else {
      puVar3[0xb] = 0x10;
      puVar3[10] = 2;
      puVar3[0xd] = 2;
      puVar3[0xc] = param_2;
      if (param_2 != 0) {
        puVar3 = puVar3 + 0x10;
        uVar6 = (ulonglong)param_2;
        do {
          puVar3[1] = 0;
          *puVar3 = 0xffa00000;
          *(undefined8 *)(puVar3 + -2) = 0x8000;
          puVar3 = puVar3 + 4;
          uVar6 = uVar6 - 1;
        } while (uVar6 != 0);
      }
    }
    *(uint *)(param_1 + 0x30) = uVar5;
    uVar4 = 0;
  }
  return uVar4;
}



// === FUN_140017c04 @ 140017c04 (size=92) ===

undefined8 FUN_140017c04(longlong param_1)

{
  undefined8 uVar1;
  
  uVar1 = 0xc0000010;
  if ((*(uint *)(param_1 + 0x20) & 1) == 0) {
    if ((*(uint *)(param_1 + 0x20) >> 9 & 1) != 0) {
      uVar1 = FUN_140017970(param_1,0x201,0x13);
    }
  }
  else {
    uVar1 = FUN_140018044(param_1,4,0);
    if (-1 < (int)uVar1) {
      **(undefined4 **)(param_1 + 0x38) = 0;
      *(undefined4 *)(param_1 + 0x30) = 4;
      return uVar1;
    }
  }
  return uVar1;
}



// === FUN_140017c60 @ 140017c60 (size=316) ===

ulonglong FUN_140017c60(ulonglong param_1,longlong *param_2,longlong param_3,uint param_4)

{
  int *piVar1;
  uint uVar2;
  int iVar3;
  int *piVar4;
  ulonglong uVar5;
  
  piVar4 = (int *)FUN_14000403c(param_1);
  if ((*(uint *)(param_3 + 0x20) & 0x200) == 0) {
    uVar2 = FUN_140018044(param_3,4,4);
    iVar3 = 0;
    uVar5 = (ulonglong)uVar2;
    if (-1 < (int)uVar2) {
      piVar1 = *(int **)(param_3 + 0x38);
      uVar2 = **(uint **)(param_3 + 0x28);
      if ((uVar2 < param_4) || (uVar2 == 0xffffffff)) {
        if ((*(uint *)(param_3 + 0x20) & 1) == 0) {
          if ((*(uint *)(param_3 + 0x20) & 2) != 0) {
            if (uVar2 == 0xffffffff) {
              do {
                (*(code *)PTR__guard_dispatch_icall_140008188)
                          (param_2,*(undefined4 *)(param_3 + 0x10),iVar3,*piVar1 != 0);
                iVar3 = iVar3 + 1;
                *piVar4 = *piVar1;
                piVar4 = piVar4 + 1;
              } while (iVar3 != -1);
            }
            else {
              (*(code *)PTR__guard_dispatch_icall_140008188)
                        (param_2,*(undefined4 *)(param_3 + 0x10),uVar2,*piVar1 != 0);
              if (uVar2 < param_4) {
                piVar4[uVar2] = *piVar1;
              }
            }
          }
        }
        else {
          if (uVar2 == 0xffffffff) {
            uVar2 = 0;
          }
          iVar3 = (*(code *)PTR__guard_dispatch_icall_140008188)
                            (param_2,*(undefined4 *)(param_3 + 0x10),uVar2);
          *piVar1 = iVar3;
          *(undefined4 *)(param_3 + 0x30) = 4;
        }
      }
      else {
        uVar5 = 0xc000000d;
      }
    }
  }
  else {
    uVar5 = FUN_1400179e0(param_3,param_4);
    uVar5 = uVar5 & 0xffffffff;
  }
  return uVar5;
}



// === FUN_140017d9c @ 140017d9c (size=208) ===

int FUN_140017d9c(int param_1,undefined8 param_2,longlong param_3,uint param_4,int param_5)

{
  uint uVar1;
  int *piVar2;
  int iVar3;
  int iVar4;
  undefined *puVar5;
  
  if ((*(uint *)(param_3 + 0x20) & 0x200) != 0) {
    iVar3 = FUN_140017a90(param_3,param_4);
    return iVar3;
  }
  iVar4 = FUN_140018044(param_3,4,4);
  iVar3 = 0;
  if (iVar4 < 0) {
    return iVar4;
  }
  piVar2 = *(int **)(param_3 + 0x38);
  uVar1 = **(uint **)(param_3 + 0x28);
  if ((param_4 <= uVar1) && (uVar1 != 0xffffffff)) {
    return -0x3ffffff3;
  }
  if ((*(uint *)(param_3 + 0x20) & 1) == 0) {
    return iVar4;
  }
  puVar5 = FUN_140004080(param_1);
  if (param_5 == 0) {
    iVar3 = *(int *)(puVar5 + (ulonglong)uVar1 * 4 + 0x50);
  }
  else {
    if (param_5 != 1) goto LAB_140017e3e;
    iVar3 = *(int *)(puVar5 + (ulonglong)uVar1 * 4 + 0xd0);
  }
  if (0x800000 < iVar3) {
    iVar3 = 0x800000;
  }
LAB_140017e3e:
  *piVar2 = iVar3 << 8;
  *(undefined4 *)(param_3 + 0x30) = 4;
  return iVar4;
}



// === FUN_140017e6c @ 140017e6c (size=470) ===

int FUN_140017e6c(ulonglong param_1,longlong *param_2,longlong param_3,uint param_4)

{
  int *piVar1;
  int iVar2;
  int iVar3;
  int *piVar4;
  uint uVar5;
  int iVar6;
  
  piVar4 = (int *)FUN_14000409c(param_1);
  if ((*(uint *)(param_3 + 0x20) & 0x200) == 0) {
    iVar2 = FUN_140018044(param_3,4,4);
    iVar3 = 0;
    if (-1 < iVar2) {
      piVar1 = *(int **)(param_3 + 0x38);
      uVar5 = **(uint **)(param_3 + 0x28);
      if ((uVar5 < param_4) || (uVar5 == 0xffffffff)) {
        if ((*(uint *)(param_3 + 0x20) & 1) == 0) {
          if ((*(uint *)(param_3 + 0x20) & 2) != 0) {
            if (uVar5 == 0xffffffff) {
              iVar3 = 0;
              do {
                iVar6 = *piVar1;
                if (iVar6 < 1) {
                  if (iVar6 < -0x600000) {
                    iVar6 = -0x600000;
                  }
                  else {
                    iVar6 = ((int)((0x4000 - iVar6 >> 0x1f & 0x7fffU) + (0x4000 - iVar6)) >> 0xf) *
                            -0x8000;
                  }
                }
                else {
                  iVar6 = 0;
                }
                (*(code *)PTR__guard_dispatch_icall_140008188)
                          (param_2,*(undefined4 *)(param_3 + 0x10),iVar3,iVar6);
                iVar3 = iVar3 + 1;
                *piVar4 = *piVar1;
                piVar4 = piVar4 + 1;
              } while (iVar3 != -1);
            }
            else {
              iVar6 = *piVar1;
              if ((iVar6 < 1) && (iVar3 = -0x600000, -0x600001 < iVar6)) {
                iVar3 = ((int)((0x4000 - iVar6 >> 0x1f & 0x7fffU) + (0x4000 - iVar6)) >> 0xf) *
                        -0x8000;
              }
              (*(code *)PTR__guard_dispatch_icall_140008188)
                        (param_2,*(undefined4 *)(param_3 + 0x10),uVar5,iVar3);
              if (uVar5 < param_4) {
                piVar4[uVar5] = *piVar1;
              }
            }
          }
        }
        else {
          if (uVar5 == 0xffffffff) {
            uVar5 = 0;
          }
          iVar3 = (*(code *)PTR__guard_dispatch_icall_140008188)
                            (param_2,*(undefined4 *)(param_3 + 0x10),uVar5);
          *piVar1 = iVar3;
          *(undefined4 *)(param_3 + 0x30) = 4;
        }
      }
      else {
        iVar2 = -0x3ffffff3;
      }
    }
  }
  else {
    iVar2 = FUN_140017b4c(param_3,param_4);
  }
  return iVar2;
}



// === FUN_140018044 @ 140018044 (size=99) ===

void FUN_140018044(longlong param_1,uint param_2,uint param_3)

{
  uint uVar1;
  int iVar2;
  
  if ((param_1 == 0) || (param_2 == 0)) {
    iVar2 = -0x3ffffff3;
    if (param_1 == 0) {
      return;
    }
  }
  else {
    uVar1 = *(uint *)(param_1 + 0x30);
    if (uVar1 == 0) {
      *(uint *)(param_1 + 0x30) = param_2;
      iVar2 = -0x7ffffffb;
    }
    else if ((uVar1 < param_2) || (*(uint *)(param_1 + 0x24) < param_3)) {
      iVar2 = -0x3fffffdd;
    }
    else {
      iVar2 = -0x3fffffff;
      if (*(longlong *)(param_1 + 0x38) != 0) {
        iVar2 = 0;
      }
    }
  }
  if ((iVar2 != 0) && (iVar2 != -0x7ffffffb)) {
    *(undefined4 *)(param_1 + 0x30) = 0;
  }
  return;
}



// === FUN_1400180b0 @ 1400180b0 (size=24) ===

undefined8 FUN_1400180b0(undefined8 param_1,int param_2,undefined4 *param_3)

{
  if (param_2 == 0) {
    *param_3 = 0;
    param_3[2] = 1;
    return 0;
  }
  return 0xc0000010;
}



// === FUN_1400180d0 @ 1400180d0 (size=49) ===

undefined8 FUN_1400180d0(undefined8 param_1,undefined8 param_2,longlong param_3,uint *param_4)

{
  *param_4 = (uint)(*(int *)(param_3 + 0x48) * 10) / 1000;
  param_4[1] = (uint)(*(int *)(param_3 + 0x48) * 2000) / 1000;
  return 0;
}



// === FUN_140018104 @ 140018104 (size=79) ===

undefined8 FUN_140018104(longlong param_1,uint param_2,int *param_3)

{
  undefined8 uVar1;
  undefined *puVar2;
  int iVar3;
  
  if (param_2 < *(ushort *)(param_1 + 0xf0)) {
    puVar2 = FUN_140004080(*(int *)(param_1 + 0x120));
    iVar3 = *(int *)(puVar2 + (ulonglong)param_2 * 4 + 0xd0);
    if (0x800000 < iVar3) {
      iVar3 = 0x800000;
    }
    *param_3 = iVar3 << 8;
    uVar1 = 0;
  }
  else {
    uVar1 = 0xc000000d;
  }
  return uVar1;
}



// === FUN_140018160 @ 140018160 (size=99) ===

undefined8 FUN_140018160(longlong param_1,int param_2,int param_3,longlong param_4,uint param_5)

{
  undefined8 uVar1;
  
  if (param_2 == 0) {
    if (param_3 == 0) {
      uVar1 = FUN_1400184bc(param_1 + -8,param_4,param_5);
      return uVar1;
    }
    if (param_3 == 1) {
      uVar1 = FUN_1400183cc(param_1 + -8,param_4,param_5);
      return uVar1;
    }
    if (param_3 == 2) {
      uVar1 = FUN_14001840c(param_1 + -8,param_4,param_5);
      return uVar1;
    }
  }
  return 0xc0000010;
}



// === FUN_1400181d0 @ 1400181d0 (size=58) ===

undefined8 FUN_1400181d0(longlong param_1,int param_2,int param_3,uint *param_4)

{
  if (param_2 == 0) {
    if (param_3 == 0) {
      *param_4 = (uint)*(ushort *)(param_1 + 0xe8);
      return 0;
    }
    if ((param_3 == 1) || (param_3 == 2)) {
      *param_4 = (uint)*(ushort *)(param_1 + 0xe8);
      return 0;
    }
  }
  return 0xc0000010;
}



// === FUN_140018210 @ 140018210 (size=36) ===

undefined8 FUN_140018210(longlong param_1,int param_2,uint param_3,undefined4 *param_4)

{
  undefined4 uVar1;
  
  if (param_2 != 0) {
    return 0xc0000010;
  }
  if (param_3 == 0xffffffff) {
    uVar1 = **(undefined4 **)(param_1 + 0x58);
  }
  else {
    uVar1 = (*(undefined4 **)(param_1 + 0x58))[param_3];
  }
  *param_4 = uVar1;
  return 0;
}



// === FUN_140018240 @ 140018240 (size=40) ===

undefined8 FUN_140018240(longlong param_1,int param_2,uint param_3,int *param_4)

{
  undefined8 uVar1;
  
  if (param_2 != 0) {
    return 0xc0000010;
  }
  uVar1 = FUN_140018104(param_1 + -8,param_3,param_4);
  return uVar1;
}



// === FUN_140018270 @ 140018270 (size=36) ===

undefined8 FUN_140018270(longlong param_1,int param_2,uint param_3,undefined4 *param_4)

{
  undefined4 uVar1;
  
  if (param_2 != 0) {
    return 0xc0000010;
  }
  if (param_3 == 0xffffffff) {
    uVar1 = **(undefined4 **)(param_1 + 0x60);
  }
  else {
    uVar1 = (*(undefined4 **)(param_1 + 0x60))[param_3];
  }
  *param_4 = uVar1;
  return 0;
}



// === FUN_1400182a0 @ 1400182a0 (size=92) ===

undefined8 FUN_1400182a0(longlong param_1,int param_2,undefined8 *param_3,uint param_4)

{
  undefined8 *puVar1;
  undefined8 uVar2;
  
  if (param_2 != 0) {
    return 0xc0000010;
  }
  if (param_4 < 0x68) {
    return 0xc0000023;
  }
  puVar1 = *(undefined8 **)(param_1 + 0x78);
  uVar2 = puVar1[1];
  *param_3 = *puVar1;
  param_3[1] = uVar2;
  uVar2 = puVar1[3];
  param_3[2] = puVar1[2];
  param_3[3] = uVar2;
  uVar2 = puVar1[5];
  param_3[4] = puVar1[4];
  param_3[5] = uVar2;
  uVar2 = puVar1[7];
  param_3[6] = puVar1[6];
  param_3[7] = uVar2;
  uVar2 = puVar1[9];
  param_3[8] = puVar1[8];
  param_3[9] = uVar2;
  uVar2 = puVar1[0xb];
  param_3[10] = puVar1[10];
  param_3[0xb] = uVar2;
  param_3[0xc] = puVar1[0xc];
  return 0;
}



// === FUN_140018300 @ 140018300 (size=91) ===

undefined8 FUN_140018300(longlong param_1,int param_2,int param_3,int *param_4)

{
  int iVar1;
  undefined8 uVar2;
  
  uVar2 = 0;
  if (param_2 == 0) {
    if ((param_3 == 0) || (param_3 == 1)) {
      *param_4 = 0x68;
    }
    else if (param_3 == 2) {
      iVar1 = FUN_140004bcc(param_1 + -8,(undefined8 *)0x0);
      *param_4 = iVar1 * 0x68 + 8;
    }
    else {
      uVar2 = 0xc000000d;
    }
  }
  else {
    uVar2 = 0xc0000010;
  }
  return uVar2;
}



// === FUN_140018360 @ 140018360 (size=9) ===

undefined8 FUN_140018360(longlong param_1,undefined8 param_2,undefined4 *param_3)

{
  *param_3 = *(undefined4 *)(param_1 + 0x50);
  return 0;
}



// === FUN_140018370 @ 140018370 (size=92) ===

undefined8 FUN_140018370(longlong param_1,int param_2,undefined8 *param_3,uint param_4)

{
  undefined8 *puVar1;
  undefined8 uVar2;
  
  if (param_2 != 0) {
    return 0xc0000010;
  }
  if (param_4 < 0x68) {
    return 0xc0000023;
  }
  puVar1 = *(undefined8 **)(param_1 + 0x70);
  uVar2 = puVar1[1];
  *param_3 = *puVar1;
  param_3[1] = uVar2;
  uVar2 = puVar1[3];
  param_3[2] = puVar1[2];
  param_3[3] = uVar2;
  uVar2 = puVar1[5];
  param_3[4] = puVar1[4];
  param_3[5] = uVar2;
  uVar2 = puVar1[7];
  param_3[6] = puVar1[6];
  param_3[7] = uVar2;
  uVar2 = puVar1[9];
  param_3[8] = puVar1[8];
  param_3[9] = uVar2;
  uVar2 = puVar1[0xb];
  param_3[10] = puVar1[10];
  param_3[0xb] = uVar2;
  param_3[0xc] = puVar1[0xc];
  return 0;
}



// === FUN_1400183cc @ 1400183cc (size=64) ===

undefined8 FUN_1400183cc(longlong param_1,longlong param_2,uint param_3)

{
  undefined4 *puVar1;
  ulonglong uVar2;
  
  if ((uint)*(ushort *)(param_1 + 0xf0) < param_3 >> 4) {
    return 0xc000000d;
  }
  if (param_3 >> 4 != 0) {
    puVar1 = (undefined4 *)(param_2 + 8);
    uVar2 = (ulonglong)(param_3 >> 4);
    do {
      *puVar1 = 0;
      puVar1[-2] = 1;
      puVar1[1] = 1;
      puVar1 = puVar1 + 4;
      uVar2 = uVar2 - 1;
    } while (uVar2 != 0);
  }
  return 0;
}



// === FUN_14001840c @ 14001840c (size=67) ===

undefined8 FUN_14001840c(longlong param_1,longlong param_2,uint param_3)

{
  undefined4 *puVar1;
  ulonglong uVar2;
  
  if ((uint)*(ushort *)(param_1 + 0xf0) < param_3 >> 4) {
    return 0xc000000d;
  }
  if (param_3 >> 4 != 0) {
    puVar1 = (undefined4 *)(param_2 + 8);
    uVar2 = (ulonglong)(param_3 >> 4);
    do {
      puVar1[-2] = 0x1000;
      puVar1[1] = 0x7fffffff;
      *puVar1 = 0x80000000;
      puVar1 = puVar1 + 4;
      uVar2 = uVar2 - 1;
    } while (uVar2 != 0);
  }
  return 0;
}



// === FUN_140018450 @ 140018450 (size=108) ===

undefined8 FUN_140018450(longlong param_1,int param_2,uint *param_3,uint param_4)

{
  uint uVar1;
  uint uVar2;
  undefined8 uVar3;
  undefined8 *local_18 [2];
  
  uVar3 = 0;
  if (param_2 == 0) {
    uVar1 = FUN_140004bcc(param_1 + -8,local_18);
    param_3[1] = uVar1;
    uVar2 = uVar1 * 0x68 + 8;
    *param_3 = uVar2;
    if (param_4 < uVar2) {
      uVar3 = 0xc0000023;
    }
    else {
      FUN_140007680((undefined8 *)(param_3 + 2),local_18[0],(ulonglong)uVar1 * 0x68);
    }
  }
  else {
    uVar3 = 0xc0000010;
  }
  return uVar3;
}



// === FUN_1400184bc @ 1400184bc (size=64) ===

undefined8 FUN_1400184bc(longlong param_1,longlong param_2,uint param_3)

{
  undefined4 *puVar1;
  ulonglong uVar2;
  
  if ((uint)*(ushort *)(param_1 + 0xf0) < param_3 >> 4) {
    return 0xc000000d;
  }
  if (param_3 >> 4 != 0) {
    puVar1 = (undefined4 *)(param_2 + 8);
    uVar2 = (ulonglong)(param_3 >> 4);
    do {
      puVar1[1] = 0;
      puVar1[-2] = 0x8000;
      *puVar1 = 0xffa00000;
      puVar1 = puVar1 + 4;
      uVar2 = uVar2 - 1;
    } while (uVar2 != 0);
  }
  return 0;
}



// === FUN_1400184fc @ 1400184fc (size=62) ===

undefined8 FUN_1400184fc(longlong param_1,uint param_2,undefined4 param_3)

{
  ulonglong uVar1;
  uint uVar2;
  ulonglong uVar3;
  
  if (param_2 == 0xffffffff) {
    uVar1 = 0;
    uVar3 = uVar1;
    if (*(short *)(param_1 + 0xf0) != 0) {
      do {
        uVar2 = (int)uVar3 + 1;
        *(undefined4 *)(uVar1 + *(longlong *)(param_1 + 0x60)) = param_3;
        uVar1 = uVar1 + 4;
        uVar3 = (ulonglong)uVar2;
      } while ((int)uVar2 < (int)(uint)*(ushort *)(param_1 + 0xf0));
    }
  }
  else {
    *(undefined4 *)(*(longlong *)(param_1 + 0x60) + (ulonglong)param_2 * 4) = param_3;
  }
  return 0;
}



// === FUN_14001853c @ 14001853c (size=131) ===

undefined8 FUN_14001853c(longlong param_1,uint param_2,int param_3)

{
  ulonglong uVar1;
  int iVar2;
  uint uVar3;
  ulonglong uVar4;
  
  uVar1 = 0;
  iVar2 = 0;
  if ((param_3 < 1) && (iVar2 = -0x600000, -0x600001 < param_3)) {
    iVar2 = ((int)((0x4000 - param_3 >> 0x1f & 0x7fffU) + (0x4000 - param_3)) >> 0xf) * -0x8000;
  }
  if (param_2 == 0xffffffff) {
    uVar4 = uVar1;
    if (*(short *)(param_1 + 0xf0) != 0) {
      do {
        uVar3 = (int)uVar4 + 1;
        *(int *)(uVar1 + *(longlong *)(param_1 + 0x68)) = iVar2;
        uVar1 = uVar1 + 4;
        uVar4 = (ulonglong)uVar3;
      } while ((int)uVar3 < (int)(uint)*(ushort *)(param_1 + 0xf0));
    }
  }
  else {
    *(int *)(*(longlong *)(param_1 + 0x68) + (ulonglong)param_2 * 4) = iVar2;
  }
  return 0;
}



// === FUN_1400185c0 @ 1400185c0 (size=40) ===

undefined8 FUN_1400185c0(longlong param_1,int param_2,uint param_3,undefined4 param_4)

{
  undefined8 uVar1;
  
  if (param_2 != 0) {
    return 0xc0000010;
  }
  uVar1 = FUN_1400184fc(param_1 + -8,param_3,param_4);
  return uVar1;
}



// === FUN_1400185f0 @ 1400185f0 (size=83) ===

undefined8 FUN_1400185f0(longlong param_1,int param_2,uint param_3,int param_4)

{
  int iVar1;
  undefined8 uVar2;
  
  iVar1 = 0;
  if (param_2 != 0) {
    return 0xc0000010;
  }
  if ((param_4 < 1) && (iVar1 = -0x600000, -0x600001 < param_4)) {
    iVar1 = ((int)((0x4000 - param_4) + (0x4000 - param_4 >> 0x1f & 0x7fffU)) >> 0xf) * -0x8000;
  }
  uVar2 = FUN_14001853c(param_1 + -8,param_3,iVar1);
  return uVar2;
}



// === FUN_140018650 @ 140018650 (size=92) ===

undefined8 FUN_140018650(longlong param_1,int param_2,undefined8 *param_3,uint param_4)

{
  undefined8 *puVar1;
  undefined8 uVar2;
  
  if (param_2 != 0) {
    return 0xc0000010;
  }
  if (param_4 < 0x68) {
    return 0xc0000023;
  }
  uVar2 = param_3[1];
  puVar1 = *(undefined8 **)(param_1 + 0x78);
  *puVar1 = *param_3;
  puVar1[1] = uVar2;
  uVar2 = param_3[3];
  puVar1[2] = param_3[2];
  puVar1[3] = uVar2;
  uVar2 = param_3[5];
  puVar1[4] = param_3[4];
  puVar1[5] = uVar2;
  uVar2 = param_3[7];
  puVar1[6] = param_3[6];
  puVar1[7] = uVar2;
  uVar2 = param_3[9];
  puVar1[8] = param_3[8];
  puVar1[9] = uVar2;
  uVar2 = param_3[0xb];
  puVar1[10] = param_3[10];
  puVar1[0xb] = uVar2;
  puVar1[0xc] = param_3[0xc];
  return 0;
}



// === FUN_1400186b0 @ 1400186b0 (size=7) ===

undefined8 FUN_1400186b0(longlong param_1,undefined8 param_2,undefined4 param_3)

{
  *(undefined4 *)(param_1 + 0x50) = param_3;
  return 0;
}



// === FUN_1400186b8 @ 1400186b8 (size=17) ===

undefined8 FUN_1400186b8(longlong param_1,int param_2)

{
  if (param_2 != *(int *)(param_1 + 0x110)) {
    *(int *)(param_1 + 0x110) = param_2;
  }
  return 0;
}



// === FUN_1400186cc @ 1400186cc (size=118) ===

undefined8 FUN_1400186cc(longlong param_1,uint param_2,int *param_3)

{
  undefined *puVar1;
  int iVar2;
  
  if (param_2 < *(ushort *)(*(longlong *)(param_1 + 0x148) + 2)) {
    puVar1 = FUN_140004080(*(int *)(*(longlong *)(param_1 + 0x98) + 0x120));
    if (*(char *)(param_1 + 0xa4) == '\0') {
      iVar2 = *(int *)(puVar1 + (ulonglong)param_2 * 4 + 0x50);
    }
    else {
      iVar2 = *(int *)(puVar1 + (ulonglong)param_2 * 4 + 0xd0);
    }
    if (0x800000 < iVar2) {
      iVar2 = 0x800000;
    }
    iVar2 = iVar2 << 8;
  }
  else {
    iVar2 = 0;
  }
  *param_3 = iVar2;
  return 0;
}



// === FUN_140018750 @ 140018750 (size=11) ===

undefined8 FUN_140018750(longlong param_1,undefined4 *param_2)

{
  *param_2 = *(undefined4 *)(param_1 + 0x108);
  return 0;
}



// === FUN_14001875c @ 14001875c (size=68) ===

undefined8 FUN_14001875c(longlong param_1,longlong param_2,uint param_3)

{
  undefined4 *puVar1;
  ulonglong uVar2;
  
  if (param_3 >> 4 != (uint)*(ushort *)(*(longlong *)(param_1 + 0x148) + 2)) {
    return 0xc000000d;
  }
  if (param_3 >> 4 != 0) {
    puVar1 = (undefined4 *)(param_2 + 8);
    uVar2 = (ulonglong)(param_3 >> 4);
    do {
      *puVar1 = 0;
      puVar1[-2] = 1;
      puVar1[1] = 1;
      puVar1 = puVar1 + 4;
      uVar2 = uVar2 - 1;
    } while (uVar2 != 0);
  }
  return 0;
}



// === FUN_1400187a0 @ 1400187a0 (size=71) ===

undefined8 FUN_1400187a0(longlong param_1,longlong param_2,uint param_3)

{
  undefined4 *puVar1;
  ulonglong uVar2;
  
  if (param_3 >> 4 != (uint)*(ushort *)(*(longlong *)(param_1 + 0x148) + 2)) {
    return 0xc000000d;
  }
  if (param_3 >> 4 != 0) {
    puVar1 = (undefined4 *)(param_2 + 8);
    uVar2 = (ulonglong)(param_3 >> 4);
    do {
      puVar1[-2] = 0x1000;
      puVar1[1] = 0x7fffffff;
      *puVar1 = 0x80000000;
      puVar1 = puVar1 + 4;
      uVar2 = uVar2 - 1;
    } while (uVar2 != 0);
  }
  return 0;
}



// === FUN_1400187f0 @ 1400187f0 (size=68) ===

undefined8 FUN_1400187f0(longlong param_1,int param_2,longlong param_3,uint param_4)

{
  undefined8 uVar1;
  
  if (param_2 == 0) {
    uVar1 = FUN_1400188ec(param_1 + -0x20,param_3,param_4);
    return uVar1;
  }
  if (param_2 != 1) {
    if (param_2 != 2) {
      return 0xc0000010;
    }
    uVar1 = FUN_1400187a0(param_1 + -0x20,param_3,param_4);
    return uVar1;
  }
  uVar1 = FUN_14001875c(param_1 + -0x20,param_3,param_4);
  return uVar1;
}



// === FUN_140018840 @ 140018840 (size=59) ===

undefined8 FUN_140018840(longlong param_1,int param_2,uint *param_3)

{
  undefined8 uVar1;
  
  uVar1 = 0;
  if (param_2 == 0) {
    *param_3 = (uint)*(ushort *)(*(longlong *)(param_1 + 0x128) + 2);
  }
  else if ((param_2 == 1) || (param_2 == 2)) {
    *param_3 = (uint)*(ushort *)(*(longlong *)(param_1 + 0x128) + 2);
  }
  else {
    uVar1 = 0xc0000010;
  }
  return uVar1;
}



// === FUN_140018880 @ 140018880 (size=18) ===

undefined8 FUN_140018880(longlong param_1,uint param_2,undefined4 *param_3)

{
  *param_3 = *(undefined4 *)(*(longlong *)(param_1 + 0x110) + (ulonglong)param_2 * 4);
  return 0;
}



// === FUN_1400188a0 @ 1400188a0 (size=9) ===

void FUN_1400188a0(longlong param_1,uint param_2,int *param_3)

{
  FUN_1400186cc(param_1 + -0x20,param_2,param_3);
  return;
}



// === FUN_1400188b0 @ 1400188b0 (size=18) ===

undefined8 FUN_1400188b0(longlong param_1,uint param_2,undefined4 *param_3)

{
  *param_3 = *(undefined4 *)(*(longlong *)(param_1 + 0x118) + (ulonglong)param_2 * 4);
  return 0;
}



// === FUN_1400188d0 @ 1400188d0 (size=15) ===

void FUN_1400188d0(longlong param_1,undefined8 *param_2)

{
  FUN_140004598(param_1 + -0x20,param_2,(undefined8 *)0x0,(ulonglong *)0x0);
  return;
}



// === FUN_1400188e0 @ 1400188e0 (size=9) ===

void FUN_1400188e0(longlong param_1,ulonglong *param_2)

{
  FUN_140004664(param_1 + -0x20,param_2);
  return;
}



// === FUN_1400188ec @ 1400188ec (size=68) ===

undefined8 FUN_1400188ec(longlong param_1,longlong param_2,uint param_3)

{
  undefined4 *puVar1;
  ulonglong uVar2;
  
  if (param_3 >> 4 != (uint)*(ushort *)(*(longlong *)(param_1 + 0x148) + 2)) {
    return 0xc000000d;
  }
  if (param_3 >> 4 != 0) {
    puVar1 = (undefined4 *)(param_2 + 8);
    uVar2 = (ulonglong)(param_3 >> 4);
    do {
      puVar1[1] = 0;
      puVar1[-2] = 0x8000;
      *puVar1 = 0xffa00000;
      puVar1 = puVar1 + 4;
      uVar2 = uVar2 - 1;
    } while (uVar2 != 0);
  }
  return 0;
}



// === FUN_140018930 @ 140018930 (size=9) ===

undefined8 FUN_140018930(longlong param_1,undefined4 param_2)

{
  *(undefined4 *)(param_1 + 0x108) = param_2;
  return 0;
}



// === FUN_140018940 @ 140018940 (size=84) ===

ulonglong FUN_140018940(longlong param_1,uint param_2,undefined4 param_3)

{
  uint uVar1;
  ulonglong uVar2;
  ulonglong uVar3;
  
  if (param_2 == 0xffffffff) {
    uVar2 = 0;
    uVar3 = 0xc0000010;
    if (*(short *)(*(longlong *)(param_1 + 0x128) + 2) != 0) {
      do {
        *(undefined4 *)(*(longlong *)(param_1 + 0x110) + uVar2 * 4) = param_3;
        uVar1 = (int)uVar2 + 1;
        uVar2 = (ulonglong)uVar1;
        uVar3 = 0;
      } while (uVar1 < *(ushort *)(*(longlong *)(param_1 + 0x128) + 2));
    }
  }
  else {
    *(undefined4 *)(*(longlong *)(param_1 + 0x110) + (ulonglong)param_2 * 4) = param_3;
    uVar3 = 0;
  }
  return uVar3;
}



// === FUN_1400189a0 @ 1400189a0 (size=159) ===

ulonglong FUN_1400189a0(longlong param_1,uint param_2,int param_3)

{
  int iVar1;
  uint uVar2;
  ulonglong uVar3;
  ulonglong uVar4;
  
  iVar1 = 0;
  if ((param_3 < 1) && (iVar1 = -0x600000, -0x600001 < param_3)) {
    iVar1 = ((int)((0x4000 - param_3 >> 0x1f & 0x7fffU) + (0x4000 - param_3)) >> 0xf) * -0x8000;
  }
  if (param_2 == 0xffffffff) {
    uVar3 = 0;
    uVar4 = 0xc0000010;
    if (*(short *)(*(longlong *)(param_1 + 0x128) + 2) != 0) {
      do {
        *(int *)(*(longlong *)(param_1 + 0x118) + uVar3 * 4) = iVar1;
        uVar2 = (int)uVar3 + 1;
        uVar3 = (ulonglong)uVar2;
        uVar4 = 0;
      } while (uVar2 < *(ushort *)(*(longlong *)(param_1 + 0x128) + 2));
    }
  }
  else {
    *(int *)(*(longlong *)(param_1 + 0x118) + (ulonglong)param_2 * 4) = iVar1;
    uVar4 = 0;
  }
  return uVar4;
}



// === FUN_140018a40 @ 140018a40 (size=9) ===

void FUN_140018a40(longlong param_1,uint param_2)

{
  FUN_140004700(param_1 + -0x20,param_2);
  return;
}



// === FUN_140018a50 @ 140018a50 (size=9) ===

void FUN_140018a50(longlong param_1,int param_2)

{
  FUN_1400186b8(*(longlong *)(param_1 + 0x78),param_2);
  return;
}



// === FUN_140018a60 @ 140018a60 (size=15) ===

void FUN_140018a60(longlong *param_1)

{
  FUN_140018f60((uint *)(*param_1 + 0x20),param_1);
  return;
}



// === FUN_140018a70 @ 140018a70 (size=25) ===

undefined4 *
FUN_140018a70(undefined4 *param_1,undefined4 param_2,undefined8 param_3,undefined2 param_4)

{
  *(undefined8 *)(param_1 + 2) = 0;
  *(undefined8 *)(param_1 + 6) = 0;
  *param_1 = param_2;
  *(undefined8 *)(param_1 + 4) = param_3;
  *(undefined2 *)(param_1 + 8) = param_4;
  return param_1;
}



// === FUN_140018a8c @ 140018a8c (size=52) ===

void FUN_140018a8c(undefined8 *param_1)

{
  *param_1 = &PTR_FUN_140008ee8;
  param_1[1] = &PTR_FUN_140008f18;
  FUN_140006e20(param_1 + 1);
  FUN_140018ac0((longlong)(param_1 + 4));
  return;
}



// === FUN_140018ac0 @ 140018ac0 (size=69) ===

void FUN_140018ac0(longlong param_1)

{
  if (*(longlong *)(param_1 + 8) != 0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)();
    *(undefined8 *)(param_1 + 8) = 0;
  }
  if (*(longlong *)(param_1 + 0x18) != 0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)();
    *(undefined8 *)(param_1 + 0x18) = 0;
  }
  return;
}



// === FUN_140018b10 @ 140018b10 (size=156) ===

undefined8
FUN_140018b10(undefined4 param_1,undefined8 *param_2,undefined8 param_3,undefined8 *param_4,
             undefined8 param_5,undefined8 param_6,undefined8 param_7,undefined4 *param_8)

{
  undefined8 *puVar1;
  
  puVar1 = (undefined8 *)FUN_1400048a8((undefined1 *)0x260,param_5);
  if ((puVar1 != (undefined8 *)0x0) &&
     (puVar1 = FUN_1400048d0(puVar1,param_1,param_4,*(undefined8 *)(param_8 + 8),
                             *(undefined2 *)(param_8 + 0x1a),*param_8,param_7),
     puVar1 != (undefined8 *)0x0)) {
    (*(code *)PTR__guard_dispatch_icall_140008188)(puVar1);
    *param_2 = puVar1;
    return 0;
  }
  return 0xc000009a;
}



// === FUN_140018bb0 @ 140018bb0 (size=6) ===

undefined8 FUN_140018bb0(void)

{
  return 0xc0000002;
}



// === FUN_140018bc0 @ 140018bc0 (size=10) ===

undefined8 FUN_140018bc0(longlong param_1,undefined8 *param_2)

{
  *param_2 = *(undefined8 *)(param_1 + 0x30);
  return 0;
}



// === FUN_140018bd0 @ 140018bd0 (size=14) ===

void FUN_140018bd0(longlong param_1,undefined8 *param_2,undefined8 param_3,undefined8 *param_4)

{
  FUN_140018be0(param_1 + 0x20,param_2,param_4);
  return;
}



// === FUN_140018be0 @ 140018be0 (size=195) ===

int FUN_140018be0(longlong param_1,undefined8 *param_2,undefined8 *param_3)

{
  longlong *plVar1;
  int iVar2;
  longlong *plVar3;
  
  plVar1 = (longlong *)(param_1 + 8);
  iVar2 = (*(code *)PTR__guard_dispatch_icall_140008188)(param_2,&DAT_140008a40,plVar1);
  plVar3 = (longlong *)(param_1 + 0x18);
  if ((-1 < iVar2) &&
     (iVar2 = (*(code *)PTR__guard_dispatch_icall_140008188)(param_3,&DAT_1400089e0,plVar3),
     -1 < iVar2)) {
    (*(code *)PTR__guard_dispatch_icall_140008188)();
    return iVar2;
  }
  if (*plVar1 != 0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)();
    *plVar1 = 0;
  }
  if (*plVar3 != 0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)();
    *plVar3 = 0;
  }
  return iVar2;
}



// === FUN_140018cb0 @ 140018cb0 (size=119) ===

undefined8 FUN_140018cb0(longlong param_1,longlong *param_2,undefined8 *param_3)

{
  longlong lVar1;
  
  lVar1 = *param_2;
  if ((((lVar1 == DAT_1400088e0) && (param_2[1] == DAT_1400088e8)) ||
      ((lVar1 == DAT_140008900 && (param_2[1] == DAT_140008908)))) ||
     ((lVar1 == DAT_140008920 && (param_2[1] == DAT_140008928)))) {
    *param_3 = param_1 + -8;
    if (param_1 + -8 != 0) {
      (*(code *)PTR__guard_dispatch_icall_140008188)();
      return 0;
    }
  }
  else {
    *param_3 = 0;
  }
  return 0xc000000d;
}



// === FUN_140018d28 @ 140018d28 (size=568) ===

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

ulonglong FUN_140018d28(longlong param_1,longlong param_2)

{
  undefined8 uVar1;
  uint uVar2;
  undefined4 uVar3;
  ulonglong uVar4;
  undefined8 uVar5;
  int iVar6;
  undefined4 *puVar7;
  
  iVar6 = *(int *)(param_2 + 0x10);
  if ((*(uint *)(param_2 + 0x20) & 0x200) != 0) {
    if (iVar6 == 9) {
      uVar4 = FUN_140017970(param_2,0x203,0xb);
      return uVar4 & 0xffffffff;
    }
    if (iVar6 == 10) {
      iVar6 = 3;
    }
    else {
      if (iVar6 != 0xb) {
        return 0xc000000d;
      }
      iVar6 = 0x13;
    }
    uVar2 = *(uint *)(param_2 + 0x30);
    if (uVar2 != 0) {
      if (uVar2 < 0x28) {
        if (uVar2 < 4) {
          *(undefined4 *)(param_2 + 0x30) = 0;
          return 0xc0000023;
        }
        **(undefined4 **)(param_2 + 0x38) = 0x203;
        *(undefined4 *)(param_2 + 0x30) = 4;
      }
      else {
        puVar7 = *(undefined4 **)(param_2 + 0x38);
        uVar3 = 0x40;
        *puVar7 = 0x203;
        puVar7[1] = 0x40;
        uVar1 = _UNK_140008e28;
        uVar5 = _DAT_140008e20;
        puVar7[7] = 0;
        puVar7[8] = 0;
        puVar7[9] = 0;
        *(undefined8 *)(puVar7 + 2) = uVar5;
        *(undefined8 *)(puVar7 + 4) = uVar1;
        puVar7[6] = iVar6;
        if (*(uint *)(param_2 + 0x30) < 0x40) {
          uVar3 = 0x28;
        }
        else {
          puVar7[0xd] = 0;
          puVar7[8] = 1;
          puVar7[10] = 1;
          puVar7[0xc] = 1;
          puVar7[0xb] = 8;
          puVar7[0xe] = 0;
          if (iVar6 == 3) {
            puVar7[0xf] = 0x7fffffff;
          }
          else {
            puVar7[0xf] = 0xffffffff;
          }
        }
        *(undefined4 *)(param_2 + 0x30) = uVar3;
      }
      return 0;
    }
    *(undefined4 *)(param_2 + 0x30) = 0x40;
    return 0x80000005;
  }
  if (iVar6 == 9) {
    uVar2 = FUN_140018044(param_2,4,0);
    uVar4 = (ulonglong)uVar2;
    if ((int)uVar2 < 0) {
      return uVar4;
    }
    puVar7 = *(undefined4 **)(param_2 + 0x38);
    if ((*(uint *)(param_2 + 0x20) & 1) != 0) {
LAB_140018f17:
      uVar3 = (*(code *)PTR__guard_dispatch_icall_140008188)();
      *puVar7 = uVar3;
      *(undefined4 *)(param_2 + 0x30) = 4;
      return uVar4;
    }
    if ((*(uint *)(param_2 + 0x20) & 2) != 0) {
      uVar5 = *(undefined8 *)(param_1 + 8);
LAB_140018f38:
      (*(code *)PTR__guard_dispatch_icall_140008188)(uVar5,*puVar7);
      return uVar4;
    }
  }
  else if (iVar6 == 10) {
    uVar2 = FUN_140018044(param_2,4,0);
    uVar4 = (ulonglong)uVar2;
    if ((int)uVar2 < 0) {
      return uVar4;
    }
    puVar7 = *(undefined4 **)(param_2 + 0x38);
    if ((*(uint *)(param_2 + 0x20) & 1) != 0) goto LAB_140018f17;
    if ((*(uint *)(param_2 + 0x20) & 2) != 0) {
      uVar5 = *(undefined8 *)(param_1 + 8);
      goto LAB_140018f38;
    }
  }
  else if (iVar6 == 0xb) {
    uVar2 = FUN_140018044(param_2,4,0);
    uVar4 = (ulonglong)uVar2;
    if ((int)uVar2 < 0) {
      return uVar4;
    }
    puVar7 = *(undefined4 **)(param_2 + 0x38);
    if ((*(uint *)(param_2 + 0x20) & 1) != 0) goto LAB_140018f17;
    if ((*(uint *)(param_2 + 0x20) & 2) != 0) {
      uVar5 = *(undefined8 *)(param_1 + 8);
      goto LAB_140018f38;
    }
  }
  return 0xc000000d;
}



// === FUN_140018f60 @ 140018f60 (size=161) ===

int FUN_140018f60(uint *param_1,longlong *param_2)

{
  int iVar1;
  int iVar2;
  undefined8 uVar3;
  ulonglong uVar4;
  
  iVar2 = -0x3ffffff0;
  iVar1 = *(int *)(param_2[3] + 8);
  if (iVar1 == 4) {
    iVar2 = FUN_140017e6c((ulonglong)*param_1,*(longlong **)(param_1 + 2),(longlong)param_2,
                          (uint)(ushort)param_1[8]);
  }
  else if (iVar1 == 0xc) {
    uVar3 = FUN_140019004((longlong)param_1,(longlong)param_2);
    iVar2 = (int)uVar3;
  }
  else if (iVar1 == 0xd) {
    uVar4 = FUN_140017c60((ulonglong)*param_1,*(longlong **)(param_1 + 2),(longlong)param_2,
                          (uint)(ushort)param_1[8]);
    iVar2 = (int)uVar4;
  }
  else if (iVar1 == 0x1c) {
    uVar4 = FUN_140018d28((longlong)param_1,(longlong)param_2);
    iVar2 = (int)uVar4;
  }
  else if (iVar1 == 0x21) {
    uVar3 = FUN_140017c04((longlong)param_2);
    iVar2 = (int)uVar3;
  }
  else if (iVar1 == 0x37) {
    iVar2 = FUN_140017d9c(*param_1,*(undefined8 *)(param_1 + 2),(longlong)param_2,
                          (uint)(ushort)param_1[8],*(int *)(*param_2 + 0x48));
  }
  return iVar2;
}



// === FUN_140019004 @ 140019004 (size=141) ===

undefined8 FUN_140019004(longlong param_1,longlong param_2)

{
  uint uVar1;
  undefined4 *puVar2;
  undefined4 uVar3;
  undefined8 uVar4;
  
  if (*(uint *)(param_2 + 0x30) < 4) {
    uVar4 = 0xc000000d;
  }
  else {
    puVar2 = *(undefined4 **)(param_2 + 0x38);
    uVar1 = *(uint *)(param_2 + 0x20);
    if ((uVar1 & 1) == 0) {
      if ((uVar1 & 2) == 0) {
        if ((uVar1 >> 9 & 1) == 0) {
          return 0xc0000010;
        }
        uVar4 = FUN_140017970(param_2,0x203,3);
        return uVar4;
      }
      (*(code *)PTR__guard_dispatch_icall_140008188)(*(undefined8 *)(param_1 + 8),*puVar2);
    }
    else {
      uVar3 = (*(code *)PTR__guard_dispatch_icall_140008188)();
      *puVar2 = uVar3;
      *(undefined4 *)(param_2 + 0x30) = 4;
    }
    uVar4 = 0;
  }
  return uVar4;
}



// === FUN_140019094 @ 140019094 (size=329) ===

undefined8 *
FUN_140019094(undefined8 *param_1,undefined4 param_2,undefined8 param_3,undefined4 *param_4,
             undefined8 param_5)

{
  undefined8 *puVar1;
  undefined4 uVar2;
  undefined4 uVar3;
  undefined4 uVar4;
  undefined8 uVar5;
  
  FUN_140006e00(param_1 + 3,(undefined8 *)0x0);
  *param_1 = &PTR_FUN_140008f38;
  param_1[1] = &PTR_FUN_140008f78;
  param_1[2] = &PTR_FUN_140009010;
  param_1[3] = &PTR_FUN_140009030;
  *(undefined8 *)((longlong)param_1 + 0x3c) = 0;
  param_1[0x1b] = *(undefined8 *)(param_4 + 0x1c);
  *(undefined4 *)((longlong)param_1 + 0xec) = param_4[0x1e];
  *(undefined2 *)(param_1 + 0x1e) = *(undefined2 *)(param_4 + 0x1a);
  param_1[0x23] = param_5;
  *(undefined4 *)(param_1 + 0x26) = param_4[0x23];
  *(undefined4 *)((longlong)param_1 + 0x134) = *param_4;
  param_1[0x28] = 0;
  param_1[0x29] = param_4;
  param_1[0x25] = param_3;
  *(undefined4 *)(param_1 + 0x24) = param_2;
  puVar1 = *(undefined8 **)(param_4 + 0x14);
  if (puVar1 != (undefined8 *)0x0) {
    uVar5 = puVar1[1];
    param_1[0x11] = *puVar1;
    param_1[0x12] = uVar5;
    uVar5 = puVar1[3];
    param_1[0x13] = puVar1[2];
    param_1[0x14] = uVar5;
    uVar5 = puVar1[5];
    param_1[0x15] = puVar1[4];
    param_1[0x16] = uVar5;
    uVar5 = puVar1[7];
    param_1[0x17] = puVar1[6];
    param_1[0x18] = uVar5;
    uVar2 = *(undefined4 *)((longlong)puVar1 + 0x44);
    uVar3 = *(undefined4 *)(puVar1 + 9);
    uVar4 = *(undefined4 *)((longlong)puVar1 + 0x4c);
    *(undefined4 *)(param_1 + 0x19) = *(undefined4 *)(puVar1 + 8);
    *(undefined4 *)((longlong)param_1 + 0xcc) = uVar2;
    *(undefined4 *)(param_1 + 0x1a) = uVar3;
    *(undefined4 *)((longlong)param_1 + 0xd4) = uVar4;
    *(undefined4 *)((longlong)param_1 + 0x3c) = 4;
  }
  *(undefined4 *)(param_1 + 8) = 1;
  KeInitializeSpinLock(param_1 + 0x1c);
  *(undefined1 *)(param_1 + 0x1d) = 0;
  return param_1;
}



// === FUN_1400191e0 @ 1400191e0 (size=342) ===

void FUN_1400191e0(undefined8 *param_1)

{
  *param_1 = &PTR_FUN_140008f38;
  param_1[1] = &PTR_FUN_140008f78;
  param_1[2] = &PTR_FUN_140009010;
  param_1[3] = &PTR_FUN_140009030;
  if (param_1[0x10] != 0) {
    FUN_140003a54(param_1[0x10]);
    param_1[0x10] = 0;
  }
  if (param_1[0xf] != 0) {
    FUN_140003a54(param_1[0xf]);
    param_1[0xf] = 0;
  }
  if (param_1[0xc] != 0) {
    FUN_140003a54(param_1[0xc]);
    param_1[0xc] = 0;
  }
  if (param_1[0xd] != 0) {
    FUN_140003a54(param_1[0xd]);
    param_1[0xd] = 0;
  }
  if (param_1[0xe] != 0) {
    FUN_140003a54(param_1[0xe]);
    param_1[0xe] = 0;
  }
  if (param_1[0x1f] != 0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)();
    param_1[0x1f] = 0;
  }
  if (param_1[0x27] != 0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)();
    param_1[0x27] = 0;
  }
  if (param_1[0x28] != 0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)();
    param_1[0x28] = 0;
  }
  if (param_1[9] != 0) {
    FUN_140003a54(param_1[9]);
    param_1[9] = 0;
  }
  if (param_1[10] != 0) {
    FUN_140003a54(param_1[10]);
    param_1[10] = 0;
  }
  FUN_140006e20(param_1 + 3);
  return;
}



// === FUN_140019340 @ 140019340 (size=118) ===

undefined8
FUN_140019340(undefined4 param_1,undefined8 *param_2,undefined8 param_3,undefined8 param_4,
             undefined8 param_5,undefined8 param_6,undefined8 param_7,undefined4 *param_8)

{
  undefined8 *puVar1;
  
  puVar1 = (undefined8 *)FUN_1400048a8((undefined1 *)0x158,param_5);
  if ((puVar1 != (undefined8 *)0x0) &&
     (puVar1 = FUN_140019094(puVar1,param_1,param_6,param_8,param_7), puVar1 != (undefined8 *)0x0))
  {
    (*(code *)PTR__guard_dispatch_icall_140008188)(puVar1);
    *param_2 = puVar1;
    return 0;
  }
  return 0xc000009a;
}



// === FUN_1400193c0 @ 1400193c0 (size=90) ===

undefined8
FUN_1400193c0(undefined8 param_1,undefined8 param_2,longlong param_3,longlong param_4,uint param_5,
             undefined8 param_6,undefined4 *param_7)

{
  undefined8 uVar1;
  
  if ((*(longlong *)(param_3 + 0x30) != DAT_140008e00) ||
     (*(longlong *)(param_3 + 0x38) != DAT_140008e08)) {
    return 0xc0000002;
  }
  if (param_5 == 0) {
    *param_7 = 0x52;
    return 0x80000005;
  }
  if (param_5 < 0x52) {
    return 0xc0000023;
  }
  uVar1 = 0xc0000002;
  if (*(int *)(param_4 + 0x40) != *(int *)(param_3 + 0x40)) {
    uVar1 = 0xc0000272;
  }
  return uVar1;
}



// === FUN_14001941c @ 14001941c (size=131) ===

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined8 FUN_14001941c(longlong param_1,ulonglong param_2,undefined8 *param_3)

{
  undefined8 uVar1;
  ulonglong uVar2;
  int *piVar3;
  ulonglong uVar4;
  uint uVar5;
  
  uVar1 = _UNK_140009210;
  *param_3 = _DAT_140009208;
  param_3[1] = uVar1;
  if (param_2 < 8) {
    return 0xc000000d;
  }
  uVar5 = 0;
  uVar4 = param_2 - 8;
  piVar3 = (int *)(param_1 + 8);
  if (*(int *)(param_1 + 4) != 0) {
    do {
      if (uVar4 < 0x18) {
        return 0xc000000d;
      }
      if ((*(longlong *)(piVar3 + 2) != DAT_140009218) ||
         (*(longlong *)(piVar3 + 4) != DAT_140009220)) {
        return 0xc00000bb;
      }
      if (uVar4 < 0x28) {
        return 0xc000000d;
      }
      if (*piVar3 != 0x28) {
        return 0xc000000d;
      }
      uVar1 = *(undefined8 *)(piVar3 + 8);
      uVar5 = uVar5 + 1;
      *param_3 = *(undefined8 *)(piVar3 + 6);
      param_3[1] = uVar1;
      uVar2 = (ulonglong)(*piVar3 + 7) & 0xfffffff8;
      piVar3 = (int *)((longlong)piVar3 + uVar2);
      uVar4 = uVar4 - uVar2;
    } while (uVar5 < *(uint *)(param_1 + 4));
  }
  return 0;
}



// === FUN_1400194a0 @ 1400194a0 (size=13) ===

undefined8 FUN_1400194a0(longlong param_1,longlong *param_2)

{
  *param_2 = param_1 + 0x88;
  return 0;
}



// === FUN_1400194b0 @ 1400194b0 (size=52) ===

undefined8 FUN_1400194b0(undefined8 param_1,longlong *param_2)

{
  FUN_140007940(param_2,0,(undefined1 *)0x40);
  *(undefined4 *)(param_2 + 4) = 0xffffffff;
  *(undefined2 *)((longlong)param_2 + 4) = 0x101;
  *(undefined1 *)(param_2 + 1) = 1;
  *(undefined4 *)((longlong)param_2 + 0x14) = 5;
  return 0;
}



// === FUN_1400194f0 @ 1400194f0 (size=145) ===

undefined8 FUN_1400194f0(longlong param_1,uint param_2,undefined8 *param_3,uint *param_4)

{
  undefined8 uVar1;
  undefined8 uVar2;
  uint uVar3;
  undefined8 uVar4;
  ulonglong uVar5;
  undefined8 *local_res8;
  
  uVar4 = 0;
  local_res8 = (undefined8 *)0x0;
  if (param_2 < *(uint *)(*(longlong *)(*(longlong *)(param_1 + 0x138) + 0x50) + 0x14)) {
    uVar3 = FUN_140004c44(param_1 + -0x10,param_2,&local_res8);
    if (uVar3 == 0) {
      uVar4 = 0xc00000bb;
    }
    else {
      if (param_3 != (undefined8 *)0x0) {
        if (*param_4 < uVar3) {
          uVar4 = 0xc0000023;
        }
        else if (uVar3 != 0) {
          uVar5 = (ulonglong)uVar3;
          do {
            uVar1 = *local_res8;
            uVar2 = local_res8[1];
            local_res8 = local_res8 + 3;
            *param_3 = uVar1;
            param_3[1] = uVar2;
            param_3 = param_3 + 2;
            uVar5 = uVar5 - 1;
          } while (uVar5 != 0);
        }
      }
      *param_4 = uVar3;
    }
  }
  else {
    uVar4 = 0xc000000d;
  }
  return uVar4;
}



// === FUN_140019590 @ 140019590 (size=846) ===

undefined8 FUN_140019590(longlong param_1,undefined8 param_2,undefined8 param_3,undefined8 *param_4)

{
  undefined8 *puVar1;
  longlong lVar2;
  undefined4 uVar3;
  undefined4 uVar4;
  undefined4 uVar5;
  int iVar6;
  longlong *plVar7;
  undefined8 uVar8;
  undefined8 *local_res8;
  
  *(undefined8 *)(param_1 + 0x30) = 0;
  *(undefined4 *)(param_1 + 0x38) = 0;
  *(undefined8 *)(param_1 + 0x48) = 0;
  *(undefined8 *)(param_1 + 0x50) = 0;
  *(undefined4 *)(param_1 + 0x58) = 0;
  *(undefined8 *)(param_1 + 0x60) = 0;
  *(undefined8 *)(param_1 + 0x68) = 0;
  *(undefined8 *)(param_1 + 0x70) = 0;
  *(undefined8 *)(param_1 + 0x78) = 0;
  *(undefined8 *)(param_1 + 0x80) = 0;
  *(undefined8 *)(param_1 + 0x10c) = 0;
  *(undefined8 *)(param_1 + 0x100) = 0;
  *(undefined4 *)(param_1 + 0x108) = 0;
  *(undefined4 *)(param_1 + 0x150) = 0x10;
  *(undefined4 *)(param_1 + 0x114) = 3;
  iVar6 = (*(code *)PTR__guard_dispatch_icall_140008188)
                    (param_4,&DAT_140008c80,(undefined8 *)(param_1 + 0x140));
  if (iVar6 < 0) {
    *(undefined8 *)(param_1 + 0x140) = 0;
  }
  if (*(int *)(param_1 + 0x134) == 0) {
    if (*(uint *)(param_1 + 0x3c) == 0) {
      return 0xc0000184;
    }
    plVar7 = FUN_140003a04(0x40,(undefined1 *)((ulonglong)*(uint *)(param_1 + 0x3c) << 3));
    *(longlong **)(param_1 + 0x48) = plVar7;
    if (plVar7 != (longlong *)0x0) {
      if ((*(uint *)(param_1 + 0x130) & 2) != 0) {
        if (*(uint *)(param_1 + 0x40) == 0) {
          return 0xc0000184;
        }
        plVar7 = FUN_140003a04(0x40,(undefined1 *)((ulonglong)*(uint *)(param_1 + 0x40) << 3));
        *(longlong **)(param_1 + 0x50) = plVar7;
        if (plVar7 == (longlong *)0x0) goto LAB_1400198d0;
      }
      plVar7 = FUN_140003a04(0x40,(undefined1 *)((ulonglong)*(ushort *)(param_1 + 0xf0) << 2));
      *(longlong **)(param_1 + 0x60) = plVar7;
      if (plVar7 != (longlong *)0x0) {
        plVar7 = FUN_140003a04(0x40,(undefined1 *)((ulonglong)*(ushort *)(param_1 + 0xf0) << 2));
        *(longlong **)(param_1 + 0x68) = plVar7;
        if (plVar7 != (longlong *)0x0) {
          plVar7 = FUN_140003a04(0x40,(undefined1 *)((ulonglong)*(ushort *)(param_1 + 0xf0) << 2));
          *(longlong **)(param_1 + 0x70) = plVar7;
          if (plVar7 != (longlong *)0x0) {
            plVar7 = FUN_140003a04(0x40,(undefined1 *)0x68);
            *(longlong **)(param_1 + 0x80) = plVar7;
            if (plVar7 != (longlong *)0x0) {
              iVar6 = FUN_140004bcc(param_1,&local_res8);
              if (iVar6 == 0) {
                return 0xc0000001;
              }
              puVar1 = *(undefined8 **)(param_1 + 0x80);
              uVar8 = local_res8[1];
              *puVar1 = *local_res8;
              puVar1[1] = uVar8;
              uVar8 = local_res8[3];
              puVar1[2] = local_res8[2];
              puVar1[3] = uVar8;
              uVar8 = local_res8[5];
              puVar1[4] = local_res8[4];
              puVar1[5] = uVar8;
              uVar8 = local_res8[7];
              puVar1[6] = local_res8[6];
              puVar1[7] = uVar8;
              uVar8 = local_res8[9];
              puVar1[8] = local_res8[8];
              puVar1[9] = uVar8;
              uVar8 = local_res8[0xb];
              puVar1[10] = local_res8[10];
              puVar1[0xb] = uVar8;
              puVar1[0xc] = local_res8[0xc];
              plVar7 = FUN_140003a04(0x40,(undefined1 *)0x68);
              *(longlong **)(param_1 + 0x78) = plVar7;
              if (plVar7 != (longlong *)0x0) {
                *(undefined4 *)plVar7 = 0x68;
                *(undefined4 *)(*(longlong *)(param_1 + 0x78) + 4) = 0;
                *(undefined4 *)(*(longlong *)(param_1 + 0x78) + 0xc) = 0;
                *(undefined4 *)(*(longlong *)(param_1 + 0x78) + 8) = 0;
                uVar8 = DAT_140008df8;
                lVar2 = *(longlong *)(param_1 + 0x78);
                *(undefined8 *)(lVar2 + 0x10) = DAT_140008df0;
                *(undefined8 *)(lVar2 + 0x18) = uVar8;
                uVar5 = DAT_140009200._4_4_;
                uVar4 = (undefined4)DAT_140009200;
                uVar3 = DAT_1400091f8._4_4_;
                lVar2 = *(longlong *)(param_1 + 0x78);
                *(undefined4 *)(lVar2 + 0x20) = (undefined4)DAT_1400091f8;
                *(undefined4 *)(lVar2 + 0x24) = uVar3;
                *(undefined4 *)(lVar2 + 0x28) = uVar4;
                *(undefined4 *)(lVar2 + 0x2c) = uVar5;
                uVar8 = DAT_140008e08;
                lVar2 = *(longlong *)(param_1 + 0x78);
                *(undefined8 *)(lVar2 + 0x30) = DAT_140008e00;
                *(undefined8 *)(lVar2 + 0x38) = uVar8;
                *(undefined2 *)(*(longlong *)(param_1 + 0x78) + 0x40) = 0xfffe;
                *(undefined2 *)(*(longlong *)(param_1 + 0x78) + 0x42) = 2;
                *(undefined4 *)(*(longlong *)(param_1 + 0x78) + 0x44) = 48000;
                *(undefined2 *)(*(longlong *)(param_1 + 0x78) + 0x4c) = 4;
                *(undefined4 *)(*(longlong *)(param_1 + 0x78) + 0x48) = 0x2ee00;
                *(undefined2 *)(*(longlong *)(param_1 + 0x78) + 0x4e) = 0x10;
                *(undefined2 *)(*(longlong *)(param_1 + 0x78) + 0x50) = 0x16;
                uVar5 = DAT_140009200._4_4_;
                uVar4 = (undefined4)DAT_140009200;
                uVar3 = DAT_1400091f8._4_4_;
                lVar2 = *(longlong *)(param_1 + 0x78);
                *(undefined4 *)(lVar2 + 0x58) = (undefined4)DAT_1400091f8;
                *(undefined4 *)(lVar2 + 0x5c) = uVar3;
                *(undefined4 *)(lVar2 + 0x60) = uVar4;
                *(undefined4 *)(lVar2 + 100) = uVar5;
                *(undefined2 *)(*(longlong *)(param_1 + 0x78) + 0x52) = 0x10;
                *(undefined4 *)(*(longlong *)(param_1 + 0x78) + 0x54) = 3;
                *(undefined4 *)(param_1 + 0x58) = 0;
                iVar6 = (*(code *)PTR__guard_dispatch_icall_140008188)
                                  (param_4,&DAT_1400089f0,(undefined8 *)(param_1 + 0xf8));
                if (iVar6 < 0) {
                  *(undefined8 *)(param_1 + 0xf8) = 0;
                }
                goto LAB_14001988f;
              }
            }
          }
        }
      }
    }
LAB_1400198d0:
    uVar8 = 0xc000009a;
  }
  else {
LAB_14001988f:
    iVar6 = (*(code *)PTR__guard_dispatch_icall_140008188)(param_4,&DAT_1400089e0,param_1 + 0x138);
    if (iVar6 < 0) {
      *(undefined8 *)(param_1 + 0x138) = 0;
    }
    uVar8 = 0;
  }
  return uVar8;
}



// === FUN_1400198e0 @ 1400198e0 (size=129) ===

undefined4 FUN_1400198e0(longlong param_1,uint param_2,undefined8 param_3,longlong param_4)

{
  undefined4 uVar1;
  
  uVar1 = 0xc0000272;
  if (*(uint *)(*(longlong *)(*(longlong *)(param_1 + 0x148) + 0x50) + 0x14) <= param_2) {
    return 0xc000000d;
  }
  if ((((short *)(param_4 + 0x40) != (short *)0x0) &&
      ((*(short *)(param_4 + 0x40) == 1 ||
       (((*(short *)(param_4 + 0x50) == 0x16 && (*(longlong *)(param_4 + 0x58) == DAT_1400091f8)) &&
        (*(longlong *)(param_4 + 0x60) == DAT_140009200)))))) &&
     ((((ushort)(*(short *)(param_4 + 0x42) - 1U) < 0x10 &&
       (*(int *)(param_4 + 0x44) - 0x2b11U < 0x2c2f0)) &&
      (uVar1 = 0xc0000272, (ushort)(*(short *)(param_4 + 0x4e) - 0x10U) < 9)))) {
    uVar1 = 0;
  }
  return uVar1;
}



// === FUN_140019970 @ 140019970 (size=426) ===

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

ulonglong FUN_140019970(longlong *param_1,undefined8 *param_2,undefined8 *param_3,uint param_4,
                       char param_5,int *param_6)

{
  undefined4 uVar1;
  undefined4 uVar2;
  undefined4 uVar3;
  undefined4 uVar4;
  uint uVar5;
  ulonglong uVar6;
  undefined8 *puVar7;
  undefined8 *puVar8;
  ulonglong uVar9;
  uint *puVar10;
  undefined7 uVar11;
  undefined8 local_48;
  undefined4 uStack_40;
  undefined4 uStack_3c;
  
  puVar7 = param_3;
  if ((int)param_1[0x24] == 0) {
    FUN_140004d3c((longlong)param_1,param_4);
  }
  uStack_3c = _UNK_140009214;
  uStack_40 = _UNK_140009210;
  local_48._4_4_ = _UNK_14000920c;
  local_48._0_4_ = _DAT_140009208;
  puVar8 = (undefined8 *)0x0;
  *param_2 = 0;
  if ((param_6[1] & 2U) == 0) {
LAB_1400199fb:
    uVar4 = uStack_3c;
    uVar3 = uStack_40;
    uVar2 = local_48._4_4_;
    uVar1 = (undefined4)local_48;
    uVar11 = (undefined7)((ulonglong)puVar7 >> 8);
    uVar5 = FUN_14001a4d4((longlong)param_1,param_4,param_5);
    uVar9 = (ulonglong)uVar5;
    if (-1 < (int)uVar5) {
      uVar5 = FUN_1400198e0((longlong)param_1,param_4,CONCAT71(uVar11,param_5),(longlong)param_6);
      uVar9 = (ulonglong)uVar5;
      if (-1 < (int)uVar5) {
        puVar7 = (undefined8 *)FUN_1400048a8((undefined1 *)0x1e0,0x40);
        if ((puVar7 == (undefined8 *)0x0) ||
           (puVar8 = FUN_1400049f4(puVar7,(undefined8 *)0x0), puVar8 == (undefined8 *)0x0)) {
          uVar9 = 0xc000009a;
          goto LAB_140019adb;
        }
        (*(code *)PTR__guard_dispatch_icall_140008188)(puVar8);
        local_48._0_4_ = uVar1;
        local_48._4_4_ = uVar2;
        uStack_40 = uVar3;
        uStack_3c = uVar4;
        uVar9 = FUN_14001abac((longlong)puVar8,param_1,param_3,param_4,param_5,(longlong)param_6,
                              (undefined4 *)&local_48);
        uVar9 = uVar9 & 0xffffffff;
      }
    }
  }
  else {
    puVar7 = &local_48;
    puVar10 = (uint *)(((ulonglong)(*param_6 + 7) & 0xfffffff8) + (longlong)param_6);
    uVar6 = FUN_14001941c((longlong)puVar10,(ulonglong)*puVar10,puVar7);
    uVar9 = uVar6 & 0xffffffff;
    if (-1 < (int)uVar6) goto LAB_1400199fb;
  }
  if (-1 < (int)uVar9) {
    *param_2 = -(ulonglong)(puVar8 != (undefined8 *)0x0) & (ulonglong)(puVar8 + 1);
    (*(code *)PTR__guard_dispatch_icall_140008188)();
  }
  if (puVar8 != (undefined8 *)0x0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)(puVar8);
  }
LAB_140019adb:
  if ((int)param_1[0x24] == 0) {
    FUN_140004d3c((longlong)param_1,param_4);
  }
  return uVar9;
}



// === FUN_140019b20 @ 140019b20 (size=157) ===

undefined8 FUN_140019b20(longlong param_1,longlong *param_2,undefined8 *param_3)

{
  longlong lVar1;
  ulonglong uVar2;
  
  lVar1 = *param_2;
  if ((((lVar1 == DAT_1400088e0) && (param_2[1] == DAT_1400088e8)) ||
      ((lVar1 == DAT_140008900 && (param_2[1] == DAT_140008908)))) ||
     ((lVar1 == DAT_140008940 && (param_2[1] == DAT_140008948)))) {
    uVar2 = param_1 - 0x18;
  }
  else {
    if ((lVar1 != DAT_140008c70) || (param_2[1] != DAT_140008c78)) {
      *param_3 = 0;
      return 0xc000000d;
    }
    uVar2 = -(ulonglong)(param_1 != 0x18) & param_1 - 8U;
  }
  *param_3 = uVar2;
  if (uVar2 == 0) {
    return 0xc000000d;
  }
  (*(code *)PTR__guard_dispatch_icall_140008188)();
  return 0;
}



// === FUN_140019bc0 @ 140019bc0 (size=282) ===

int FUN_140019bc0(longlong param_1,longlong param_2)

{
  int *piVar1;
  int iVar2;
  int iVar3;
  
  iVar2 = FUN_140018044(param_2,4,0);
  if (iVar2 < 0) {
    return iVar2;
  }
  piVar1 = *(int **)(param_2 + 0x38);
  if ((*(uint *)(param_2 + 0x20) & 1) == 0) {
    if ((*(uint *)(param_2 + 0x20) & 2) == 0) {
      return iVar2;
    }
    iVar3 = *piVar1;
    if (iVar3 == 3) {
      if (*(uint *)(param_1 + 0x150) < 2) {
        return -0x3fffff45;
      }
      *piVar1 = 3;
      iVar3 = 3;
    }
    else if (iVar3 != 4) {
      if (iVar3 == 0x33) {
        if (*(uint *)(param_1 + 0x150) < 4) {
          return -0x3fffff45;
        }
        iVar3 = 0x33;
      }
      else if (iVar3 == 0x3f) {
        if (*(uint *)(param_1 + 0x150) < 6) {
          return -0x3fffff45;
        }
        iVar3 = 0x3f;
      }
      else if (iVar3 == 0xff) {
        if (*(uint *)(param_1 + 0x150) < 8) {
          return -0x3fffff45;
        }
        iVar3 = 0xff;
      }
      else if (iVar3 == 0x107) {
        if (*(uint *)(param_1 + 0x150) < 4) {
          return -0x3fffff45;
        }
        iVar3 = 0x107;
      }
      else if (iVar3 == 0x60f) {
        if (*(uint *)(param_1 + 0x150) < 6) {
          return -0x3fffff45;
        }
        iVar3 = 0x60f;
      }
      else {
        if ((iVar3 != 0x63f) || (*(uint *)(param_1 + 0x150) < 8)) {
          return -0x3fffff45;
        }
        iVar3 = 0x63f;
      }
      *piVar1 = iVar3;
    }
    *(int *)(param_1 + 0x114) = iVar3;
    return iVar2;
  }
  *piVar1 = *(int *)(param_1 + 0x114);
  *(undefined4 *)(param_2 + 0x30) = 4;
  return iVar2;
}



// === FUN_140019cdc @ 140019cdc (size=427) ===

/* WARNING: Function: __security_check_cookie replaced with injection: security_check_cookie */

undefined8 FUN_140019cdc(longlong param_1,longlong param_2)

{
  uint *puVar1;
  undefined8 *puVar2;
  uint uVar3;
  undefined8 uVar4;
  uint uVar5;
  longlong *plVar6;
  ulonglong uVar7;
  longlong *plVar8;
  uint *puVar9;
  undefined1 auStack_58 [32];
  longlong *local_38;
  longlong local_30;
  longlong lStack_28;
  ulonglong local_20;
  
  local_20 = DAT_140012be0 ^ (ulonglong)auStack_58;
  local_38 = (longlong *)0x0;
  local_30 = 0;
  lStack_28 = 0;
  if (7 < *(uint *)(param_2 + 0x24)) {
    puVar9 = *(uint **)(param_2 + 0x28);
    uVar3 = *puVar9;
    if (uVar3 < *(uint *)(*(longlong *)(*(longlong *)(param_1 + 0x148) + 0x50) + 0x14)) {
      uVar3 = FUN_140004c44(param_1,uVar3,&local_38);
      plVar8 = local_38;
      if (local_38 == (longlong *)0x0) {
        return 0xc00000bb;
      }
      uVar5 = 0;
      if (uVar3 == 0) {
        return 0xc00000bb;
      }
      plVar6 = local_38 + 2;
      while (*plVar6 == 0) {
        uVar5 = uVar5 + 1;
        plVar6 = plVar6 + 3;
        if (uVar3 <= uVar5) {
          return 0xc00000bb;
        }
      }
      if ((*(uint *)(param_2 + 0x20) & 0x200) != 0) {
        uVar4 = FUN_140017970(param_2,*(undefined4 *)(*(longlong *)(param_2 + 0x18) + 0xc),0xffff);
        return uVar4;
      }
      puVar9 = puVar9 + 2;
      uVar7 = ((ulonglong)*(uint *)(param_2 + 0x24) - (longlong)puVar9) +
              *(longlong *)(param_2 + 0x28);
      uVar4 = FUN_14001941c((longlong)puVar9,uVar7,&local_30);
      if ((int)uVar4 < 0) {
        return uVar4;
      }
      uVar5 = 0;
      while ((*plVar8 != local_30 || (plVar8[1] != lStack_28))) {
        uVar5 = uVar5 + 1;
        plVar8 = plVar8 + 3;
        if (uVar3 <= uVar5) {
          return 0xc00000bb;
        }
      }
      puVar1 = (uint *)plVar8[2];
      if (puVar1 == (uint *)0x0) {
        return 0xc00000bb;
      }
      puVar2 = *(undefined8 **)(param_2 + 0x38);
      uVar3 = *puVar1 + 7 & 0xfffffff8;
      if (0xffffffff < uVar7) {
        return 0xc000000d;
      }
      uVar5 = uVar3 + (int)uVar7;
      if (uVar5 < uVar3) {
        return 0xc000000d;
      }
      if (uVar5 == 0) {
        return 0xc00000bb;
      }
      if (*(uint *)(param_2 + 0x30) == 0) {
        *(uint *)(param_2 + 0x30) = uVar5;
        return 0x80000005;
      }
      if (uVar5 <= *(uint *)(param_2 + 0x30)) {
        if ((*(uint *)(param_2 + 0x20) & 1) != 0) {
          FUN_140007680(puVar2,(undefined8 *)puVar1,(ulonglong)*puVar1);
          *(undefined4 *)(*(longlong *)(param_2 + 0x38) + 4) = 2;
          FUN_140007680((undefined8 *)((ulonglong)uVar3 + (longlong)puVar2),(undefined8 *)puVar9,
                        uVar7);
          *(uint *)(param_2 + 0x30) = uVar5;
          return 0;
        }
        return 0xc0000010;
      }
      return 0xc0000023;
    }
  }
  return 0xc000000d;
}



// === FUN_140019e88 @ 140019e88 (size=272) ===

ulonglong FUN_140019e88(longlong param_1,longlong param_2,undefined8 param_3)

{
  uint uVar1;
  uint *puVar2;
  longlong lVar3;
  bool bVar4;
  undefined4 uVar5;
  ulonglong uVar6;
  undefined7 extraout_var;
  undefined7 extraout_var_00;
  undefined7 extraout_var_01;
  undefined7 extraout_var_02;
  undefined7 extraout_var_03;
  undefined7 extraout_var_04;
  undefined4 extraout_var_05;
  undefined1 uVar7;
  
  if (*(uint *)(param_2 + 0x24) < 8) {
    uVar6 = 0xc000000d;
  }
  else {
    puVar2 = *(uint **)(param_2 + 0x28);
    bVar4 = FUN_140004d84(param_1,*puVar2);
    if ((((int)CONCAT71(extraout_var,bVar4) == 0) &&
        (bVar4 = FUN_140004cf4(param_1,*puVar2), (int)CONCAT71(extraout_var_00,bVar4) == 0)) &&
       (bVar4 = FUN_140004d3c(param_1,*puVar2), (int)CONCAT71(extraout_var_01,bVar4) == 0)) {
      bVar4 = FUN_140004cac(param_1,*puVar2);
      return (ulonglong)((-(uint)((int)CONCAT71(extraout_var_02,bVar4) != 0) & 0xae) + 0xc000000d);
    }
    uVar1 = *(uint *)(param_2 + 0x20);
    uVar6 = 0;
    if ((uVar1 >> 9 & 1) == 0) {
      if (*(uint *)(param_2 + 0x30) == 0) {
        *(undefined4 *)(param_2 + 0x30) = 0x68;
        uVar6 = 0x80000005;
      }
      else if (*(uint *)(param_2 + 0x30) < 0x68) {
        uVar6 = 0xc0000023;
      }
      else {
        uVar7 = 1;
        if ((uVar1 & 1) == 0) {
          if ((uVar1 & 2) != 0) {
            lVar3 = *(longlong *)(param_2 + 0x38);
            bVar4 = FUN_140004d3c(param_1,*puVar2);
            if (((int)CONCAT71(extraout_var_03,bVar4) == 0) &&
               (bVar4 = FUN_140004cf4(param_1,*puVar2), (int)CONCAT71(extraout_var_04,bVar4) == 0))
            {
              uVar7 = 0;
            }
            uVar5 = FUN_1400198e0(param_1,*puVar2,CONCAT71((int7)((ulonglong)param_3 >> 8),uVar7),
                                  lVar3);
            uVar6 = CONCAT44(extraout_var_05,uVar5);
          }
        }
        else {
          uVar6 = 0xc0000010;
        }
      }
    }
    else {
      uVar6 = FUN_140017970(param_2,*(undefined4 *)(*(longlong *)(param_2 + 0x18) + 0xc),0xffff);
    }
  }
  return uVar6;
}



// === FUN_140019fa0 @ 140019fa0 (size=368) ===

int FUN_140019fa0(longlong *param_1)

{
  longlong *plVar1;
  int iVar2;
  longlong lVar3;
  longlong *plVar4;
  longlong lVar5;
  int iVar6;
  undefined8 uVar7;
  ulonglong uVar8;
  
  lVar3 = *param_1;
  iVar6 = -0x3ffffff0;
  if (lVar3 == 0) {
    iVar6 = -0x3ffffff3;
  }
  else {
    (*(code *)PTR__guard_dispatch_icall_140008188)(lVar3);
    plVar4 = (longlong *)param_1[3];
    lVar5 = *(longlong *)*plVar4;
    plVar1 = (longlong *)*plVar4 + 1;
    if ((lVar5 == DAT_1400086e8) && (*plVar1 == DAT_1400086f0)) {
      if ((int)plVar4[1] == 0xe) {
        uVar8 = FUN_140019e88(lVar3,(longlong)param_1,lVar5);
        iVar6 = (int)uVar8;
      }
      else if ((int)plVar4[1] == 0xf) {
        uVar7 = FUN_140019cdc(lVar3,(longlong)param_1);
        iVar6 = (int)uVar7;
      }
    }
    else if ((*(int *)(lVar3 + 0x134) == 0) &&
            ((lVar5 == DAT_140008490 && (*plVar1 == DAT_140008498)))) {
      iVar2 = (int)plVar4[1];
      if (iVar2 == 3) {
        iVar6 = FUN_140019bc0(lVar3,(longlong)param_1);
      }
      else if (iVar2 == 4) {
        iVar6 = FUN_140017e6c((ulonglong)*(uint *)(lVar3 + 0x120),*(longlong **)(lVar3 + 0x128),
                              (longlong)param_1,(uint)*(ushort *)(lVar3 + 0xf0));
      }
      else if (iVar2 == 0xd) {
        uVar8 = FUN_140017c60((ulonglong)*(uint *)(lVar3 + 0x120),*(longlong **)(lVar3 + 0x128),
                              (longlong)param_1,(uint)*(ushort *)(lVar3 + 0xf0));
        iVar6 = (int)uVar8;
      }
      else if (iVar2 == 0x21) {
        uVar7 = FUN_140017c04((longlong)param_1);
        iVar6 = (int)uVar7;
      }
      else if (iVar2 == 0x37) {
        iVar6 = FUN_140017d9c(*(int *)(lVar3 + 0x120),*(undefined8 *)(lVar3 + 0x128),
                              (longlong)param_1,(uint)*(ushort *)(lVar3 + 0xf0),0);
      }
    }
    (*(code *)PTR__guard_dispatch_icall_140008188)(lVar3);
  }
  return iVar6;
}



// === FUN_14001a110 @ 14001a110 (size=300) ===

undefined8 FUN_14001a110(longlong param_1,uint param_2,longlong param_3)

{
  undefined4 uVar1;
  bool bVar2;
  bool bVar3;
  bool bVar4;
  uint uVar5;
  undefined7 extraout_var;
  undefined7 extraout_var_00;
  undefined7 extraout_var_01;
  undefined *puVar6;
  undefined7 extraout_var_02;
  undefined7 extraout_var_03;
  undefined7 extraout_var_04;
  ulonglong uVar7;
  longlong *plVar8;
  uint uVar9;
  longlong *plVar10;
  
  bVar2 = false;
  plVar10 = (longlong *)0x0;
  uVar9 = 0;
  bVar3 = FUN_140004d3c(param_1,param_2);
  bVar4 = false;
  if ((int)CONCAT71(extraout_var,bVar3) == 0) {
    bVar4 = FUN_140004cf4(param_1,param_2);
    if ((int)CONCAT71(extraout_var_00,bVar4) != 0) {
      *(int *)(param_1 + 0x30) = *(int *)(param_1 + 0x30) + -1;
      plVar10 = *(longlong **)(param_1 + 0x50);
      uVar9 = *(uint *)(param_1 + 0x40);
      goto LAB_14001a17d;
    }
    bVar4 = FUN_140004d84(param_1,param_2);
    if ((int)CONCAT71(extraout_var_01,bVar4) == 0) goto LAB_14001a17d;
    bVar4 = true;
  }
  bVar2 = bVar4;
  uVar9 = *(uint *)(param_1 + 0x3c);
  plVar10 = *(longlong **)(param_1 + 0x48);
  *(int *)(param_1 + 0x34) = *(int *)(param_1 + 0x34) + -1;
LAB_14001a17d:
  puVar6 = FUN_140004080(*(int *)(param_1 + 0x120));
  if (puVar6 != (undefined *)0x0) {
    bVar4 = FUN_140004d3c(param_1,param_2);
    if ((int)CONCAT71(extraout_var_02,bVar4) == 0) {
      bVar4 = FUN_140004cf4(param_1,param_2);
      if ((int)CONCAT71(extraout_var_03,bVar4) == 0) {
        bVar4 = FUN_140004d84(param_1,param_2);
        if ((int)CONCAT71(extraout_var_04,bVar4) != 0) {
          uVar1 = *(undefined4 *)(param_1 + 0x34);
          *(undefined4 *)(puVar6 + 0xb8) = 0;
          *(undefined4 *)(puVar6 + 0xb0) = uVar1;
        }
      }
      else {
        *(undefined4 *)(puVar6 + 0xb8) = *(undefined4 *)(param_1 + 0x30);
      }
    }
    else {
      uVar1 = *(undefined4 *)(param_1 + 0x34);
      *(undefined4 *)(puVar6 + 0x138) = 0;
      *(undefined4 *)(puVar6 + 0x130) = uVar1;
    }
  }
  if ((plVar10 != (longlong *)0x0) && (uVar7 = 0, plVar8 = plVar10, uVar9 != 0)) {
    do {
      if (*plVar8 == param_3) {
        plVar10[uVar7] = 0;
        break;
      }
      uVar5 = (int)uVar7 + 1;
      uVar7 = (ulonglong)uVar5;
      plVar8 = plVar8 + 1;
    } while (uVar5 < uVar9);
  }
  if (bVar2) {
    FUN_14001a34c(param_1);
  }
  return 0;
}



// === FUN_14001a23c @ 14001a23c (size=270) ===

undefined8 FUN_14001a23c(longlong param_1,uint param_2,longlong param_3)

{
  undefined4 uVar1;
  bool bVar2;
  uint uVar3;
  undefined7 extraout_var;
  undefined7 extraout_var_00;
  undefined7 extraout_var_01;
  undefined *puVar4;
  undefined7 extraout_var_02;
  undefined7 extraout_var_03;
  undefined7 extraout_var_04;
  ulonglong uVar5;
  longlong *plVar6;
  uint uVar7;
  longlong *plVar8;
  
  plVar8 = (longlong *)0x0;
  uVar7 = 0;
  bVar2 = FUN_140004d3c(param_1,param_2);
  if ((int)CONCAT71(extraout_var,bVar2) == 0) {
    bVar2 = FUN_140004cf4(param_1,param_2);
    if ((int)CONCAT71(extraout_var_00,bVar2) != 0) {
      *(int *)(param_1 + 0x30) = *(int *)(param_1 + 0x30) + 1;
      plVar8 = *(longlong **)(param_1 + 0x50);
      uVar7 = *(uint *)(param_1 + 0x40);
      goto LAB_14001a29e;
    }
    bVar2 = FUN_140004d84(param_1,param_2);
    if ((int)CONCAT71(extraout_var_01,bVar2) == 0) goto LAB_14001a29e;
  }
  uVar7 = *(uint *)(param_1 + 0x3c);
  plVar8 = *(longlong **)(param_1 + 0x48);
  *(int *)(param_1 + 0x34) = *(int *)(param_1 + 0x34) + 1;
LAB_14001a29e:
  puVar4 = FUN_140004080(*(int *)(param_1 + 0x120));
  if (puVar4 != (undefined *)0x0) {
    bVar2 = FUN_140004d3c(param_1,param_2);
    if ((int)CONCAT71(extraout_var_02,bVar2) == 0) {
      bVar2 = FUN_140004cf4(param_1,param_2);
      if ((int)CONCAT71(extraout_var_03,bVar2) == 0) {
        bVar2 = FUN_140004d84(param_1,param_2);
        if ((int)CONCAT71(extraout_var_04,bVar2) != 0) {
          uVar1 = *(undefined4 *)(param_1 + 0x34);
          *(undefined4 *)(puVar4 + 0xb8) = 0;
          *(undefined4 *)(puVar4 + 0xb0) = uVar1;
        }
      }
      else {
        *(undefined4 *)(puVar4 + 0xb8) = *(undefined4 *)(param_1 + 0x30);
      }
    }
    else {
      uVar1 = *(undefined4 *)(param_1 + 0x34);
      *(undefined4 *)(puVar4 + 0x138) = 0;
      *(undefined4 *)(puVar4 + 0x130) = uVar1;
    }
  }
  if ((plVar8 != (longlong *)0x0) && (uVar5 = 0, plVar6 = plVar8, uVar7 != 0)) {
    do {
      if (*plVar6 == 0) {
        plVar8[uVar5] = param_3;
        return 0;
      }
      uVar3 = (int)uVar5 + 1;
      uVar5 = (ulonglong)uVar3;
      plVar6 = plVar6 + 1;
    } while (uVar3 < uVar7);
  }
  return 0;
}



// === FUN_14001a34c @ 14001a34c (size=390) ===

/* WARNING: Function: __security_check_cookie replaced with injection: security_check_cookie */

int FUN_14001a34c(longlong param_1)

{
  longlong lVar1;
  undefined1 auVar2 [16];
  int iVar3;
  undefined1 *puVar4;
  longlong lVar5;
  uint uVar6;
  ulonglong uVar7;
  ulonglong uVar8;
  bool bVar9;
  undefined1 auStack_68 [48];
  undefined4 local_38 [2];
  undefined4 local_30;
  undefined4 uStack_2c;
  undefined4 local_28;
  ulonglong local_20;
  
  local_20 = DAT_140012be0 ^ (ulonglong)auStack_68;
  local_38[0] = 0;
  local_30 = 0;
  uVar8 = 0;
  uStack_2c = 0;
  local_28 = 0;
  iVar3 = -0x3fffffff;
  if (*(longlong *)(param_1 + 0xf8) != 0) {
    auVar2 = ZEXT416(4) * ZEXT416(*(uint *)(param_1 + 0x3c));
    puVar4 = auVar2._0_8_;
    if (auVar2._8_8_ != 0) {
      puVar4 = (undefined1 *)0xffffffffffffffff;
    }
    lVar5 = FUN_1400048a8(puVar4,0x40);
    if (lVar5 == 0) {
      iVar3 = -0x3fffff66;
    }
    else {
      uVar7 = 0;
      iVar3 = -0x3fffffff;
      if (*(int *)(param_1 + 0x3c) != 0) {
        do {
          lVar1 = *(longlong *)(*(longlong *)(param_1 + 0x48) + uVar7 * 8);
          if (lVar1 != 0) {
            *(undefined4 *)(lVar5 + uVar8 * 4) = *(undefined4 *)(lVar1 + 0x150);
            uVar8 = (ulonglong)((int)uVar8 + 1);
          }
          uVar6 = (int)uVar7 + 1;
          uVar7 = (ulonglong)uVar6;
        } while (uVar6 < *(uint *)(param_1 + 0x3c));
        if ((int)uVar8 != 0) {
          iVar3 = (*(code *)PTR__guard_dispatch_icall_140008188)
                            (*(undefined8 *)(param_1 + 0xf8),lVar5,uVar8,local_38);
          bVar9 = -1 < iVar3;
          if (bVar9) {
            iVar3 = (*(code *)PTR__guard_dispatch_icall_140008188)
                              (*(undefined8 *)(param_1 + 0xf8),local_38[0],&local_30);
          }
          if (iVar3 < 0) {
            if (bVar9) {
              (*(code *)PTR__guard_dispatch_icall_140008188)
                        (*(undefined8 *)(param_1 + 0xf8),local_38[0]);
            }
          }
          else {
            (*(code *)PTR__guard_dispatch_icall_140008188)
                      (*(undefined8 *)(param_1 + 0xf8),*(undefined4 *)(param_1 + 0x10c));
            *(undefined4 *)(param_1 + 0x10c) = local_38[0];
            *(ulonglong *)(param_1 + 0x100) = CONCAT44(uStack_2c,local_30);
            *(undefined4 *)(param_1 + 0x108) = local_28;
          }
        }
      }
      FUN_1400048b8(lVar5);
    }
  }
  return iVar3;
}



// === FUN_14001a4d4 @ 14001a4d4 (size=102) ===

uint FUN_14001a4d4(longlong param_1,uint param_2,char param_3)

{
  bool bVar1;
  int iVar2;
  undefined7 extraout_var;
  undefined7 extraout_var_01;
  undefined7 extraout_var_00;
  
  if (param_3 == '\0') {
    bVar1 = FUN_140004d84(param_1,param_2);
    iVar2 = (int)CONCAT71(extraout_var_01,bVar1);
  }
  else {
    bVar1 = FUN_140004cf4(param_1,param_2);
    if ((int)CONCAT71(extraout_var,bVar1) != 0) {
      bVar1 = *(uint *)(param_1 + 0x30) < *(uint *)(param_1 + 0x40);
      goto LAB_14001a51e;
    }
    bVar1 = FUN_140004d3c(param_1,param_2);
    iVar2 = (int)CONCAT71(extraout_var_00,bVar1);
  }
  if (iVar2 == 0) {
    return 0xc00000bb;
  }
  bVar1 = *(uint *)(param_1 + 0x34) < *(uint *)(param_1 + 0x3c);
LAB_14001a51e:
  return ~-(uint)bVar1 & 0xc000009a;
}



// === FUN_14001a53c @ 14001a53c (size=381) ===

void FUN_14001a53c(undefined8 *param_1)

{
  *param_1 = &PTR_FUN_140009050;
  param_1[1] = &PTR_FUN_140009070;
  param_1[2] = &PTR_FUN_1400090e8;
  param_1[3] = &PTR_FUN_140009108;
  param_1[4] = &PTR_FUN_140009138;
  param_1[5] = &PTR_FUN_1400091b8;
  param_1[6] = &PTR_FUN_1400091d8;
  if (param_1[0x13] != 0) {
    if (*(char *)((longlong)param_1 + 0xa5) != '\0') {
      FUN_14001a110(param_1[0x13],*(uint *)(param_1 + 0x14),(longlong)param_1);
      *(undefined1 *)((longlong)param_1 + 0xa5) = 0;
    }
    (*(code *)PTR__guard_dispatch_icall_140008188)();
    param_1[0x13] = 0;
  }
  if (param_1[0x19] != 0) {
    FUN_140003a54(param_1[0x19]);
    param_1[0x19] = 0;
  }
  if (param_1[0x18] != 0) {
    FUN_140003a54(param_1[0x18]);
    param_1[0x18] = 0;
  }
  if (param_1[0xc] != 0) {
    FUN_14000669c((longlong)param_1);
    param_1[0xc] = 0;
  }
  if (param_1[0x26] != 0) {
    FUN_140003a54(param_1[0x26]);
    param_1[0x26] = 0;
  }
  if (param_1[0x27] != 0) {
    FUN_140003a54(param_1[0x27]);
    param_1[0x27] = 0;
  }
  if (param_1[0x28] != 0) {
    FUN_140003a54(param_1[0x28]);
    param_1[0x28] = 0;
  }
  if (param_1[0x29] != 0) {
    FUN_140003a54(param_1[0x29]);
    param_1[0x29] = 0;
  }
  KeFlushQueuedDpcs();
  FUN_140006e20(param_1 + 6);
  return;
}



// === FUN_14001a6c0 @ 14001a6c0 (size=423) ===

undefined8
FUN_14001a6c0(longlong param_1,uint param_2,longlong *param_3,int *param_4,undefined4 *param_5,
             undefined4 *param_6)

{
  ushort uVar1;
  uint uVar2;
  uint uVar3;
  longlong lVar4;
  undefined8 uVar5;
  undefined *puVar6;
  longlong *plVar7;
  int iVar8;
  
  if ((param_2 != 0) &&
     (uVar1 = *(ushort *)(*(longlong *)(param_1 + 0x140) + 0xc), (uint)uVar1 << 5 <= param_2)) {
    iVar8 = param_2 - param_2 % (uint)uVar1;
    lVar4 = (*(code *)PTR__guard_dispatch_icall_140008188)
                      (*(undefined8 *)(param_1 + 0x40),0xffffffff,iVar8);
    if (lVar4 != 0) {
      uVar5 = (*(code *)PTR__guard_dispatch_icall_140008188)
                        (*(undefined8 *)(param_1 + 0x40),lVar4,1);
      *(undefined4 *)(param_1 + 0xb0) = 0;
      *(undefined8 *)(param_1 + 0xa8) = uVar5;
      *(int *)(param_1 + 0xa0) = iVar8;
      puVar6 = FUN_140004080(*(int *)(*(longlong *)(param_1 + 0x90) + 0x120));
      uVar2 = *(uint *)(param_1 + 0xa0) / (uint)*(ushort *)(*(longlong *)(param_1 + 0x140) + 0xc);
      if (*(char *)(param_1 + 0x9c) == '\0') {
        *(uint *)(puVar6 + 0x150) = uVar2;
        *(undefined4 *)(puVar6 + 0x154) = *(undefined4 *)(param_1 + 0xb0);
      }
      else {
        *(uint *)(puVar6 + 0x178) = uVar2;
        *(undefined4 *)(puVar6 + 0x17c) = *(undefined4 *)(param_1 + 0xb0);
      }
      *param_3 = lVar4;
      *param_4 = iVar8;
      *param_5 = 0;
      *param_6 = 1;
      plVar7 = FUN_140003a04(0x40,(undefined1 *)(ulonglong)*(uint *)(param_1 + 0xa0));
      *(longlong **)(param_1 + 0x170) = plVar7;
      if (plVar7 != (longlong *)0x0) {
        uVar3 = ((*(uint *)(param_1 + 0xa0) /
                 (uint)*(ushort *)(*(longlong *)(param_1 + 0x140) + 0xc)) * 0x2ee00) /
                *(uint *)(*(longlong *)(param_1 + 0x140) + 4);
        uVar2 = 0xf00;
        if (0xf00 < uVar3) {
          uVar2 = uVar3;
        }
        lVar4 = FUN_140001000(uVar2 + 8,0x10,48000);
        *(longlong *)(param_1 + 0x168) = lVar4;
        if (lVar4 != 0) {
          return 0;
        }
      }
    }
  }
  return 0xc0000001;
}



// === FUN_14001a870 @ 14001a870 (size=468) ===

undefined8
FUN_14001a870(longlong param_1,uint param_2,uint param_3,longlong *param_4,uint *param_5,
             undefined4 *param_6,undefined4 *param_7)

{
  ushort uVar1;
  ulonglong uVar2;
  uint uVar3;
  longlong lVar4;
  undefined8 uVar5;
  undefined *puVar6;
  longlong *plVar7;
  uint uVar8;
  
  if (param_3 != 0) {
    uVar1 = *(ushort *)(*(longlong *)(param_1 + 0x140) + 0xc);
    if ((uint)uVar1 << 5 <= param_3) {
      if ((param_2 == 0) || (param_3 % param_2 != 0)) {
        return 0xc000000d;
      }
      uVar8 = ((param_3 / uVar1 & 1) + param_3 / uVar1) * (uint)uVar1;
      lVar4 = (*(code *)PTR__guard_dispatch_icall_140008188)
                        (*(undefined8 *)(param_1 + 0x40),0xffffffff,uVar8);
      if (lVar4 != 0) {
        uVar5 = (*(code *)PTR__guard_dispatch_icall_140008188)
                          (*(undefined8 *)(param_1 + 0x40),lVar4,1);
        *(undefined8 *)(param_1 + 0xa8) = uVar5;
        *(uint *)(param_1 + 0xb0) = param_2;
        *(uint *)(param_1 + 0xa0) = uVar8;
        *(int *)(param_1 + 0x60) =
             (int)(((ulonglong)(uVar8 * 1000) / (ulonglong)*(uint *)(param_1 + 0x11c)) /
                  (ulonglong)param_2);
        uVar2 = (ulonglong)uVar8 / (ulonglong)param_2;
        *(int *)(param_1 + 0x70) = (int)uVar2;
        *(ulonglong *)(param_1 + 0x68) = (uVar2 * 10000000) / (ulonglong)*(uint *)(param_1 + 0x11c);
        *(int *)(param_1 + 0x1b4) = (int)uVar2;
        *(int *)(param_1 + 0x1b8) = (int)(uVar2 / *(ushort *)(*(longlong *)(param_1 + 0x140) + 0xc))
        ;
        puVar6 = FUN_140004080(*(int *)(*(longlong *)(param_1 + 0x90) + 0x120));
        uVar3 = *(uint *)(param_1 + 0xa0) / (uint)*(ushort *)(*(longlong *)(param_1 + 0x140) + 0xc);
        if (*(char *)(param_1 + 0x9c) == '\0') {
          *(uint *)(puVar6 + 0x150) = uVar3;
          *(undefined4 *)(puVar6 + 0x154) = *(undefined4 *)(param_1 + 0xb0);
        }
        else {
          *(uint *)(puVar6 + 0x178) = uVar3;
          *(undefined4 *)(puVar6 + 0x17c) = *(undefined4 *)(param_1 + 0xb0);
        }
        *param_4 = lVar4;
        *param_5 = uVar8;
        *param_6 = 0;
        *param_7 = 1;
        plVar7 = FUN_140003a04(0x40,(undefined1 *)(ulonglong)*(uint *)(param_1 + 0xa0));
        *(longlong **)(param_1 + 0x170) = plVar7;
        if (plVar7 != (longlong *)0x0) {
          return 0;
        }
      }
    }
  }
  return 0xc0000001;
}



// === FUN_14001aa50 @ 14001aa50 (size=171) ===

void FUN_14001aa50(longlong param_1,longlong param_2)

{
  if (param_2 != 0) {
    if (*(longlong *)(param_1 + 0xa8) != 0) {
      (*(code *)PTR__guard_dispatch_icall_140008188)
                (*(undefined8 *)(param_1 + 0x40),*(longlong *)(param_1 + 0xa8),param_2);
      *(undefined8 *)(param_1 + 0xa8) = 0;
    }
    (*(code *)PTR__guard_dispatch_icall_140008188)(*(undefined8 *)(param_1 + 0x40),param_2);
  }
  if (*(longlong *)(param_1 + 0x170) != 0) {
    FUN_140003a54(*(longlong *)(param_1 + 0x170));
  }
  *(undefined8 *)(param_1 + 0x170) = 0;
  if (*(longlong *)(param_1 + 0x168) != 0) {
    FUN_1400010bc(*(longlong *)(param_1 + 0x168));
    *(undefined8 *)(param_1 + 0x168) = 0;
  }
  *(undefined4 *)(param_1 + 0xa0) = 0;
  *(undefined4 *)(param_1 + 0xb0) = 0;
  return;
}



// === FUN_14001ab00 @ 14001ab00 (size=160) ===

void FUN_14001ab00(longlong param_1,longlong param_2)

{
  if (param_2 != 0) {
    if (*(longlong *)(param_1 + 0xa8) != 0) {
      (*(code *)PTR__guard_dispatch_icall_140008188)
                (*(undefined8 *)(param_1 + 0x40),*(longlong *)(param_1 + 0xa8),param_2);
      *(undefined8 *)(param_1 + 0xa8) = 0;
    }
    (*(code *)PTR__guard_dispatch_icall_140008188)(*(undefined8 *)(param_1 + 0x40),param_2);
  }
  if (*(longlong *)(param_1 + 0x170) != 0) {
    FUN_140003a54(*(longlong *)(param_1 + 0x170));
  }
  *(undefined8 *)(param_1 + 0x170) = 0;
  *(undefined4 *)(param_1 + 0xa0) = 0;
  *(undefined4 *)(param_1 + 0xb0) = 0;
  *(undefined4 *)(param_1 + 0x1b4) = 0;
  *(undefined4 *)(param_1 + 0x1b8) = 0;
  return;
}



// === FUN_14001aba0 @ 14001aba0 (size=9) ===

void FUN_14001aba0(undefined8 param_1,undefined4 *param_2)

{
  *(undefined8 *)(param_2 + 1) = 0;
  *param_2 = 0;
  return;
}



// === FUN_14001abac @ 14001abac (size=778) ===

undefined8
FUN_14001abac(longlong param_1,longlong *param_2,undefined8 param_3,undefined4 param_4,
             undefined1 param_5,longlong param_6,undefined4 *param_7)

{
  longlong lVar1;
  undefined4 uVar2;
  undefined4 uVar3;
  undefined4 uVar4;
  undefined4 uVar5;
  undefined8 *puVar6;
  undefined8 uVar7;
  longlong *plVar8;
  
  *(undefined8 *)(param_1 + 0x48) = param_3;
  *(undefined8 *)(param_1 + 0x98) = 0;
  *(undefined4 *)(param_1 + 0xf0) = 0xffffffff;
  *(undefined4 *)(param_1 + 0xf4) = 0xffffffff;
  *(undefined4 *)(param_1 + 0xa0) = 0;
  *(undefined2 *)(param_1 + 0xa4) = 0;
  *(undefined4 *)(param_1 + 0xa8) = 0;
  uVar2 = *param_7;
  uVar3 = param_7[1];
  uVar4 = param_7[2];
  uVar5 = param_7[3];
  *(undefined8 *)(param_1 + 0xb0) = 0;
  lVar1 = param_1 + 0x50;
  *(undefined8 *)(param_1 + 0xb8) = 0;
  *(undefined4 *)(param_1 + 0x154) = uVar2;
  *(undefined4 *)(param_1 + 0x158) = uVar3;
  *(undefined4 *)(param_1 + 0x15c) = uVar4;
  *(undefined4 *)(param_1 + 0x160) = uVar5;
  *(undefined8 *)(param_1 + 0xc0) = 0;
  *(undefined8 *)(param_1 + 200) = 0;
  *(undefined8 *)(param_1 + 0xf8) = 0;
  *(undefined8 *)(param_1 + 0xd0) = 0;
  *(undefined8 *)(param_1 + 0xd8) = 0;
  *(undefined8 *)(param_1 + 0x108) = 0;
  *(undefined8 *)(param_1 + 0x110) = 0;
  *(undefined8 *)(param_1 + 0x118) = 0;
  *(undefined8 *)(param_1 + 0x120) = 0;
  *(undefined4 *)(param_1 + 0x128) = 0;
  *(undefined8 *)(param_1 + 0x130) = 0;
  *(undefined8 *)(param_1 + 0x138) = 0;
  *(undefined8 *)(param_1 + 0x140) = 0;
  *(undefined8 *)(param_1 + 0x148) = 0;
  *(undefined8 *)(param_1 + 0xe0) = 0;
  *(undefined8 *)(param_1 + 0xe8) = 0;
  *(undefined4 *)(param_1 + 0x150) = 0;
  *(undefined8 *)(param_1 + 0x7c) = 0;
  *(undefined2 *)(param_1 + 0x164) = 0;
  *(undefined8 *)(param_1 + 0x180) = 0;
  *(undefined8 *)(param_1 + 0x198) = 0;
  *(undefined8 *)(param_1 + 0x1a0) = 0;
  *(undefined8 *)(param_1 + 0x178) = 0;
  *(undefined8 *)(param_1 + 0x170) = 0;
  *(undefined8 *)(param_1 + 0x1b8) = 0;
  *(undefined8 *)(param_1 + 0x1d4) = 0;
  *(undefined4 *)(param_1 + 0x1c0) = 0;
  *(undefined8 *)(param_1 + 0x1c8) = 0;
  *(undefined4 *)(param_1 + 0x1d0) = 0;
  *(undefined4 *)(param_1 + 0x68) = 0;
  *(longlong *)(param_1 + 0x58) = lVar1;
  *(longlong *)lVar1 = lVar1;
  KeInitializeSpinLock(param_1 + 0x168);
  puVar6 = (undefined8 *)FUN_140017904(param_6);
  if (puVar6 == (undefined8 *)0x0) {
    uVar7 = 0xc0000001;
  }
  else {
    *(longlong **)(param_1 + 0x98) = param_2;
    if (param_2 == (longlong *)0x0) {
      uVar7 = 0xc000000d;
    }
    else {
      (*(code *)PTR__guard_dispatch_icall_140008188)(param_2);
      *(undefined4 *)(param_1 + 0xa0) = param_4;
      *(undefined1 *)(param_1 + 0xa4) = param_5;
      plVar8 = FUN_140003a04(0x40,(undefined1 *)0x40);
      *(longlong **)(param_1 + 200) = plVar8;
      if (plVar8 != (longlong *)0x0) {
        plVar8 = FUN_140003a04(0x40,(undefined1 *)((ulonglong)*(ushort *)(puVar6 + 2) + 0x12));
        *(longlong **)(param_1 + 0x148) = plVar8;
        if (plVar8 != (longlong *)0x0) {
          FUN_140007680(plVar8,puVar6,(ulonglong)*(ushort *)(puVar6 + 2) + 0x12);
          plVar8 = FUN_140003a04(0x40,(undefined1 *)
                                      ((ulonglong)*(ushort *)(*(longlong *)(param_1 + 0x148) + 2) <<
                                      2));
          *(longlong **)(param_1 + 0x130) = plVar8;
          if (plVar8 != (longlong *)0x0) {
            plVar8 = FUN_140003a04(0x40,(undefined1 *)
                                        ((ulonglong)*(ushort *)(*(longlong *)(param_1 + 0x148) + 2)
                                        << 2));
            *(longlong **)(param_1 + 0x138) = plVar8;
            if (plVar8 != (longlong *)0x0) {
              plVar8 = FUN_140003a04(0x40,(undefined1 *)
                                          ((ulonglong)
                                           *(ushort *)(*(longlong *)(param_1 + 0x148) + 2) << 2));
              *(longlong **)(param_1 + 0x140) = plVar8;
              if (plVar8 != (longlong *)0x0) {
                *(bool *)(param_1 + 0x84) = *(short *)((longlong)puVar6 + 0xe) == 0x10;
                *(undefined2 *)(param_1 + 0x86) = *(undefined2 *)((longlong)puVar6 + 0xe);
                *(undefined4 *)(param_1 + 0x8c) = *(undefined4 *)((longlong)puVar6 + 4);
                *(undefined2 *)(param_1 + 0x88) = *(undefined2 *)((longlong)puVar6 + 2);
                *(uint *)(param_1 + 0x90) = (uint)*(ushort *)((longlong)puVar6 + 0xc);
                *(undefined4 *)(param_1 + 0x124) = *(undefined4 *)(puVar6 + 1);
                FUN_14001b1c8(param_1,(longlong)puVar6);
                uVar7 = FUN_14001a23c(*(longlong *)(param_1 + 0x98),*(uint *)(param_1 + 0xa0),
                                      param_1);
                if ((int)uVar7 < 0) {
                  return uVar7;
                }
                *(undefined1 *)(param_1 + 0xa5) = 1;
                return uVar7;
              }
            }
          }
        }
      }
      uVar7 = 0xc000009a;
    }
  }
  return uVar7;
}



// === FUN_14001aec0 @ 14001aec0 (size=345) ===

undefined8 FUN_14001aec0(longlong param_1,longlong *param_2,undefined8 *param_3)

{
  longlong *plVar1;
  longlong lVar2;
  bool bVar3;
  undefined7 extraout_var;
  ulonglong uVar4;
  
  plVar1 = param_2 + 1;
  lVar2 = *param_2;
  if ((((lVar2 == DAT_1400088e0) && (*plVar1 == DAT_1400088e8)) ||
      ((lVar2 == DAT_140008950 && (*plVar1 == DAT_140008958)))) ||
     ((lVar2 == DAT_140008960 && (*plVar1 == DAT_140008968)))) {
    uVar4 = param_1 - 0x28;
  }
  else if (((lVar2 == DAT_140008990) && (*plVar1 == DAT_140008998)) &&
          (*(char *)(param_1 + 0x74) != '\0')) {
    uVar4 = param_1 - 0x20;
  }
  else if (((lVar2 == DAT_1400089a0) && (*plVar1 == DAT_1400089a8)) &&
          ((*(char *)(param_1 + 0x74) == '\0' &&
           (bVar3 = FUN_140004cf4(*(longlong *)(param_1 + 0x68),*(uint *)(param_1 + 0x70)),
           (int)CONCAT71(extraout_var,bVar3) == 0)))) {
    uVar4 = param_1 - 0x18;
  }
  else {
    lVar2 = *param_2;
    if ((lVar2 == DAT_140008c50) && (*plVar1 == DAT_140008c58)) {
      uVar4 = param_1 - 0x10;
    }
    else {
      if ((lVar2 != DAT_140008c60) || (*plVar1 != DAT_140008c68)) {
        if ((lVar2 != DAT_1400088f0) || (*plVar1 != DAT_1400088f8)) {
          *param_3 = 0;
          return 0xc000000d;
        }
        uVar4 = param_1 - 0x30;
        goto LAB_14001afde;
      }
      uVar4 = param_1 - 8;
    }
  }
  uVar4 = -(ulonglong)(param_1 != 0x30) & uVar4;
LAB_14001afde:
  *param_3 = uVar4;
  if (uVar4 == 0) {
    return 0xc000000d;
  }
  (*(code *)PTR__guard_dispatch_icall_140008188)(uVar4);
  return 0;
}



// === FUN_14001b020 @ 14001b020 (size=137) ===

undefined8 FUN_14001b020(longlong param_1,longlong param_2)

{
  undefined8 *puVar1;
  code *pcVar2;
  longlong *plVar3;
  undefined8 uVar4;
  undefined8 *puVar5;
  undefined1 *puVar6;
  undefined1 auStack_28 [8];
  undefined1 auStack_20 [24];
  
  puVar6 = auStack_28;
  plVar3 = FUN_140003a04(0x40,(undefined1 *)0x18);
  if (plVar3 == (longlong *)0x0) {
    uVar4 = 0xc000009a;
  }
  else {
    puVar1 = (undefined8 *)(param_1 + 0x48);
    plVar3[2] = param_2;
    for (puVar5 = (undefined8 *)*puVar1; puVar5 != puVar1; puVar5 = (undefined8 *)*puVar5) {
      if (puVar5[2] == param_2) goto LAB_14001b07a;
    }
    puVar5 = *(undefined8 **)(param_1 + 0x50);
    if ((undefined8 *)*puVar5 == puVar1) {
      *plVar3 = (longlong)puVar1;
      plVar3[1] = (longlong)puVar5;
      *puVar5 = plVar3;
      *(longlong **)(param_1 + 0x50) = plVar3;
      uVar4 = 0;
    }
    else {
      pcVar2 = (code *)swi(0x29);
      plVar3 = (longlong *)(*pcVar2)(3);
      puVar6 = auStack_20;
LAB_14001b07a:
      *(undefined8 *)(puVar6 + -8) = 0x14001b087;
      FUN_140003a54((longlong)plVar3);
      uVar4 = 0xc0000001;
    }
  }
  return uVar4;
}



// === FUN_14001b0b0 @ 14001b0b0 (size=54) ===

void FUN_14001b0b0(longlong param_1,undefined4 param_2)

{
  int iVar1;
  
  *(undefined4 *)(param_1 + 0x150) = param_2;
  iVar1 = FUN_14001a34c(*(longlong *)(param_1 + 0x98));
  if (iVar1 < 0) {
    *(undefined4 *)(param_1 + 0x150) = param_2;
  }
  return;
}



// === FUN_14001b0f0 @ 14001b0f0 (size=119) ===

undefined8 FUN_14001b0f0(longlong param_1,longlong param_2)

{
  longlong lVar1;
  
  if ((*(int *)(param_1 + 0xb4) != 3) && (lVar1 = FUN_140017904(param_2), lVar1 != 0)) {
    *(bool *)(param_1 + 0x7c) = *(short *)(lVar1 + 0xe) == 0x10;
    *(undefined2 *)(param_1 + 0x7e) = *(undefined2 *)(lVar1 + 0xe);
    *(undefined4 *)(param_1 + 0x11c) = *(undefined4 *)(lVar1 + 8);
    *(undefined2 *)(param_1 + 0x80) = *(undefined2 *)(lVar1 + 2);
    *(undefined4 *)(param_1 + 0x84) = *(undefined4 *)(lVar1 + 4);
    *(uint *)(param_1 + 0x88) = (uint)*(ushort *)(lVar1 + 0xc);
    FUN_14001b1c8(param_1 + -8,lVar1);
    return 0;
  }
  return 0xc00000bb;
}



// === FUN_14001b170 @ 14001b170 (size=87) ===

undefined8 FUN_14001b170(longlong param_1,longlong param_2)

{
  undefined8 *puVar1;
  undefined8 *puVar2;
  code *pcVar3;
  undefined8 *puVar4;
  undefined8 uVar5;
  
  puVar1 = *(undefined8 **)(param_1 + 0x48);
  do {
    puVar4 = puVar1;
    if (puVar4 == (undefined8 *)(param_1 + 0x48)) {
      return 0xc0000225;
    }
    puVar1 = (undefined8 *)*puVar4;
  } while (puVar4[2] != param_2);
  if (((undefined8 *)puVar1[1] == puVar4) &&
     (puVar2 = (undefined8 *)puVar4[1], (undefined8 *)*puVar2 == puVar4)) {
    *puVar2 = puVar1;
    puVar1[1] = puVar2;
    FUN_140003a54((longlong)puVar4);
    return 0;
  }
  pcVar3 = (code *)swi(0x29);
  (*pcVar3)(3);
  pcVar3 = (code *)swi(3);
  uVar5 = (*pcVar3)();
  return uVar5;
}



// === FUN_14001b1c8 @ 14001b1c8 (size=186) ===

void FUN_14001b1c8(longlong param_1,longlong param_2)

{
  bool bVar1;
  undefined *puVar2;
  undefined7 extraout_var;
  uint uVar3;
  
  puVar2 = FUN_140004080(*(int *)(*(longlong *)(param_1 + 0x98) + 0x120));
  bVar1 = FUN_140004cf4(*(longlong *)(param_1 + 0x98),*(uint *)(param_1 + 0xa0));
  uVar3 = (uint)*(ushort *)(param_2 + 2);
  if ((int)CONCAT71(extraout_var,bVar1) == 0) {
    if (*(char *)(param_1 + 0xa4) == '\0') {
      *(uint *)(puVar2 + 0xc) = uVar3;
      *(undefined4 *)(puVar2 + 0x10) = *(undefined4 *)(param_2 + 4);
      *(uint *)(puVar2 + 0x14) = (uint)*(ushort *)(param_2 + 0xe);
    }
    else {
      *(uint *)(puVar2 + 0x18) = uVar3;
      *(undefined4 *)(puVar2 + 0x1c) = *(undefined4 *)(param_2 + 4);
      *(uint *)(puVar2 + 0x20) = (uint)*(ushort *)(param_2 + 0xe);
    }
  }
  else if (*(char *)(param_1 + 0xa4) == '\0') {
    *(uint *)(puVar2 + 0xc4) = uVar3;
    *(undefined4 *)(puVar2 + 200) = *(undefined4 *)(param_2 + 4);
    *(uint *)(puVar2 + 0xcc) = (uint)*(ushort *)(param_2 + 0xe);
  }
  else {
    *(uint *)(puVar2 + 0x144) = uVar3;
    *(undefined4 *)(puVar2 + 0x148) = *(undefined4 *)(param_2 + 4);
    *(uint *)(puVar2 + 0x14c) = (uint)*(ushort *)(param_2 + 0xe);
  }
  return;
}



// === FUN_14001c000 @ 14001c000 (size=274) ===

int FUN_14001c000(longlong param_1,undefined8 param_2)

{
  int iVar1;
  undefined4 local_28;
  undefined4 local_24;
  undefined8 local_20;
  undefined8 local_18;
  undefined4 local_10;
  undefined4 local_c;
  
  local_24 = 0;
  local_18 = 0;
  local_28 = 0x20;
  local_20 = 0;
  local_10 = 2;
  local_c = 0x75417953;
  FUN_140003a68();
  iVar1 = (*(code *)PTR__guard_dispatch_icall_140008188)
                    (DAT_140013210,param_1,param_2,0,&local_28,0);
  if ((-1 < iVar1) && (iVar1 = PcInitializeAdapterDriver(param_1,param_2,FUN_140015000), -1 < iVar1)
     ) {
    *(code **)(param_1 + 0x148) = FUN_140015490;
    *(code **)(param_1 + 0x70) = FUN_140015430;
    *(code **)(param_1 + 0x80) = FUN_1400153f0;
    *(code **)(param_1 + 0xe0) = FUN_140003aa0;
    DAT_140012c98 = *(undefined8 *)(param_1 + 0x68);
    *(code **)(param_1 + 0x68) = FUN_140015630;
    return 0;
  }
  if (*DAT_140013210 != 0) {
    (*(code *)PTR__guard_dispatch_icall_140008188)();
  }
  return iVar1;
}



// === FUN_14001c114 @ 14001c114 (size=46) ===

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_14001c114(void)

{
  code *pcVar1;
  
  if ((DAT_140012be0 != 0) && (DAT_140012be0 != 0x2b992ddfa232)) {
    _DAT_140012be8 = ~DAT_140012be0;
    return;
  }
  pcVar1 = (code *)swi(0x29);
  (*pcVar1)(6);
  pcVar1 = (code *)swi(3);
  (*pcVar1)();
  return;
}



