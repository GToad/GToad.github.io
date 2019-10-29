int __fastcall sub_B4CCB784(int a1)
{
  size_t v1; // r10@1
  _BYTE *v2; // r6@1
  _BYTE *v3; // r8@1
  _BYTE *v4; // r11@1
  signed int v5; // r0@2
  size_t v6; // r2@2
  char *v7; // r1@2
  int v8; // r3@3
  int v9; // r1@7
  unsigned int v10; // r2@7
  int v11; // r3@7
  int v12; // r0@7
  int v13; // r4@9
  char v14; // r0@11
  _BYTE *v15; // r3@13
  char *v16; // r5@14
  int v17; // r5@20
  int v18; // r1@21
  int v19; // r0@22
  signed int v20; // r1@22
  int v21; // r2@23
  unsigned int v22; // r0@24
  unsigned int v23; // r8@24
  unsigned int v24; // r5@24
  _BYTE *v25; // r0@24
  int v26; // r10@25
  unsigned int v27; // r2@25
  int v28; // r12@25
  bool v29; // zf@26
  int v30; // r4@30
  int v31; // r3@30
  bool v32; // zf@32
  int v33; // r3@36
  int v34; // r1@37
  unsigned int v35; // r11@37
  unsigned int v36; // lr@38
  int v37; // r3@39
  signed int v38; // r1@45
  _BYTE *v39; // r2@45
  int v40; // r3@46
  int v41; // t1@46
  char v43; // r1@31
  unsigned int v44; // [sp+4h] [bp-234h]@24
  unsigned int v45; // [sp+8h] [bp-230h]@24
  unsigned int v46; // [sp+10h] [bp-228h]@25
  char *s; // [sp+14h] [bp-224h]@1
  char v48[256]; // [sp+18h] [bp-220h]@20
  char v49[256]; // [sp+118h] [bp-120h]@21
  int v50; // [sp+218h] [bp-20h]@1

  v50 = _stack_chk_guard;
  s = (char *)(*(int (**)(void))(*(_DWORD *)a1 + 676))();
  v1 = strlen(aUrxXItcfticust);
  v2 = malloc(v1);
  v3 = malloc(v1);
  v4 = malloc(v1);
  _aeabi_memclr(v2, v1);
  _aeabi_memclr(v3, v1);
  _aeabi_memclr(v4, v1);
  if ( v1 )
  {
    v5 = 0;
    v6 = v1;
    v7 = aUrxXItcfticust;
    do
    {
      v8 = (unsigned __int8)*v7++;
      if ( v8 != 45 )
        v3[v5++] = v8;
      --v6;
    }
    while ( v6 );
    if ( v5 >= 1 )
    {
      v9 = v5 - 1;
      v10 = -8;
      v11 = 0;
      v12 = 0;
      do
      {
        if ( (v11 | (v10 >> 2)) > 3 )
        {
          v13 = v12;
        }
        else
        {
          v13 = v12 + 1;
          v2[v12] = 45;
        }
        v14 = v3[v9--];
        v11 += 0x40000000;
        v2[v13] = v14;
        ++v10;
        v12 = v13 + 1;
      }
      while ( v9 != -1 );
      if ( v13 >= 0 )
      {
        v15 = v4;
        while ( 1 )
        {
          v16 = (char *)*v2;
          if ( (unsigned __int8)((_BYTE)v16 - 97) <= 5u )
            break;
          if ( (unsigned __int8)((_BYTE)v16 - 48) <= 9u )
          {
            v16 -= 1261644882;
            goto LABEL_18;
          }
LABEL_19:
          *v15++ = (_BYTE)v16;
          --v12;
          ++v2;
          if ( !v12 )
            goto LABEL_20;
        }
        v16 -= 1261644937;
LABEL_18:
        LOBYTE(v16) = *v16;
        goto LABEL_19;
      }
    }
  }
LABEL_20:
  _aeabi_memcpy8(v48, (const char *)&unk_B4CCD3E8, 256);
  v17 = 0;
  do
  {
    sub_B4CCBD20(v17, v1);
    v49[v17++] = v4[v18];
  }
  while ( v17 != 256 );
  v19 = (unsigned __int8)(v49[0] - 41);
  v48[0] = v48[v19];
  v48[v19] = -41;
  v20 = 1;
  do
  {
    v21 = (unsigned __int8)v48[v20];
    v19 = (v19 + (unsigned __int8)v49[v20] + v21) % 256;
    v48[v20++] = v48[v19];
    v48[v19] = v21;
  }
  while ( v20 != 256 );
  v22 = strlen(s);
  v23 = v22;
  v24 = v4[3];
  v45 = 8 * (v22 + 3 - v22 % 3);
  v44 = v24 + v45 / 6;
  v25 = malloc(v44 + 1);
  if ( !v23 )
    goto LABEL_44;
  v26 = 0;
  v27 = 0;
  v28 = 0;
  v46 = v24;
  while ( 1 )
  {
    v26 = (v26 + 1) % 256;
    v34 = (unsigned __int8)v48[v26];
    v28 = (v28 + v34) % 256;
    v48[v26] = v48[v28];
    v48[v28] = v34;
    v35 = (unsigned __int8)v48[(unsigned __int8)(v34 + v48[v26])] ^ (unsigned __int8)s[v27];
    if ( !v27 )
      break;
    v36 = 3 * (v27 / 3);
    if ( v36 == v27 )
      break;
    v29 = v27 == 1;
    if ( v27 != 1 )
      v29 = v36 + 1 == v27;
    if ( v29 )
    {
      *(&v25[v46] + v27) = byte_B4CCF050[*(&v25[v46] + v27) | (v35 >> 4)];
      v30 = (int)(&v25[v46] + v27);
      v31 = 4 * v35 & 0x3C;
      *(_BYTE *)(v30 + 1) = v31;
      if ( v27 + 1 >= v23 )
      {
        v43 = byte_B4CCF050[v31];
        *(_BYTE *)(v30 + 2) = 52;
        goto LABEL_43;
      }
    }
    else
    {
      v32 = v27 == 2;
      if ( v27 != 2 )
        v32 = v36 + 2 == v27;
      if ( v32 )
      {
        v33 = v46++ + v27;
        v25[v33] = byte_B4CCF050[v25[v33] | ((v35 & 0xC0) >> 6)] ^ 0xF;
        v25[v33 + 1] = byte_B4CCF050[v35 & 0x3F];
      }
    }
LABEL_40:
    if ( ++v27 >= v23 )
      goto LABEL_44;
  }
  *(&v25[v46] + v27) = byte_B4CCF050[v35 >> 2] ^ 7;
  v30 = (int)(&v25[v46] + v27);
  v37 = 16 * v35 & 0x30;
  *(_BYTE *)(v30 + 1) = v37;
  if ( v27 + 1 < v23 )
    goto LABEL_40;
  v43 = byte_B4CCF050[v37];
  *(_WORD *)(v30 + 2) = 15163;
LABEL_43:
  *(_BYTE *)(v30 + 1) = v43;
LABEL_44:
  if ( v45 )
  {
    v38 = 1;
    v39 = &unk_B4CCD4E8;
    do
    {
      v40 = v25[v24++];
      v41 = *v39++;
      if ( v41 != v40 )
        v38 = 0;
    }
    while ( v24 < v44 );
  }
  else
  {
    v38 = 1;
  }
  if ( _stack_chk_guard != v50 )
    _stack_chk_fail(_stack_chk_guard - v50, v38);
  return (unsigned __int8)v38;
}
