#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <memory.h>
#include <unistd.h>
#include "mex.h"
#include "matrix.h"

#ifdef _WIN32 // Some stubs for Windows
#define F_GETFL 3
void _security_check_cookie(int i) {};
int fcntl (intptr_t fd, int cmd, ...) { return 1; }
#endif //_WIN32

#define KXVER 3
#include "kx/k.h"
#define MATID    "kdbml"
#define MATLAB70 719529
#define KDB2000  10957

#define SCHK(v) v==nh ? mxGetNaN() : (v==wh || v==-wh ? mxGetInf() : v)
#define ICHK(v) v==ni ? mxGetNaN() : (v==wi || v==-wi ? mxGetInf() : v)
#define LCHK(v) v==nj ? mxGetNaN() : (v==wj || v==-wj ? mxGetInf() : v)
#define RCHK(v) v==nf ? mxGetNaN() : (v==wf || v==-wf ? mxGetInf() : v)

#define Q(e,s) {if(e) return mexErrMsgIdAndTxt(MATID,s),-1;}

double utmt(double f){return f + KDB2000 + MATLAB70;}           // matlab from kdb+ datetime
double utmtsp(double f){return f/8.64e13 + KDB2000 + MATLAB70;} // matlab from kdb+ datetimespan
K convertClass(const mxArray *pm);
K convertStructure(const mxArray *structure_array_ptr);
mxArray* maketab(K x);
mxArray* makedict(K x);
int symbol_flag = 1;
/**
* Make kdb value of given type from ordinary data
*/
K kValue(int t,void* v) {
  K r;
  switch (t) {
  case 1:  r = kb(*(I*)v); break; // bool
  case 4:  r = kg(*(I*)v); break; // byte
  case 5:  r = kh(*(I*)v); break; // short
  case 6:  r = ki(*(I*)v); break; // int
  case 7:  r = kj(*(J*)v); break; // long
  case 8:  r = ke(*(F*)v); break; // real
  case 9:  r = kf(*(F*)v); break; // real
  case 10: r = kc(*(I*)v); break; // char
  case 11: r = ks(*(S*)v); break; // symbol
  case 12: r = ktj(-KP, *(J*)v); break; // timestamp
  case 13: { r = ka(-KM); r->i = *(I*) v; } break; // month
  case 14: r = kd(*(I*)v); break; // date
  case 15: r = kz(*(F*)v); break; // datetime
  case 16: r = ktj(-KN,*(J*)v); break; // timespan
  case 17: { r = ka(-KU); r->i = *(I*)v; } break; // min ?
  case 18: { r = ka(-KV); r->i = *(I*)v; } break; // sec ?
  case 19: r = kt(*(J*)v); break; // time (msec) ?
  }
  return r;
}


/**
* Process primitives
*/
mxArray* makeatom(K x) {
  mxArray* r = NULL;
  INT32_T *mxInt32s; INT8_T *mxInt8s;INT16_T *mxInt16s;INT64_T *mxInt64s; 
  mxSingle* mxFloats;
  double* arr;
  
  switch (x->t) {
  //boolean       i.e. 0b
  case -1: r = mxCreateLogicalScalar(x->g); break;
  
  //byte          i.e. 0x00
  case -4: r = mxCreateNumericMatrix((mwSize) 1, 1, mxINT8_CLASS,mxREAL); 
    mxInt8s=mxGetInt8s (r);*mxInt8s=(x->g);break;
    
    //short         i.e. 0h
  case -5: r = mxCreateNumericMatrix((mwSize) 1, 1, mxINT16_CLASS,mxREAL); 
    mxInt16s=mxGetInt16s (r);*mxInt16s=SCHK(x->h);break;
    
    //int           i.e. 0
  case -6: r = mxCreateNumericMatrix((mwSize) 1, 1, mxINT32_CLASS,mxREAL); 
    mxInt32s=mxGetInt32s (r);*mxInt32s=ICHK(x->i);break;
    
    //long          i.e. 0j
  case -7: r = mxCreateNumericMatrix((mwSize) 1, 1, mxINT64_CLASS,mxREAL); 
    mxInt64s=mxGetInt64s (r);*mxInt64s=LCHK(x->j);break;
    
    //real          i.e. 0e
  case -8: r = mxCreateNumericMatrix((mwSize) 1, 1, mxSINGLE_CLASS,mxREAL); 
    mxFloats=mxGetSingles (r);*mxFloats=RCHK(x->e);break;
    //r = mxCreateDoubleScalar(RCHK(x->e)); break;
    
    //float         i.e. 0.0
  case -9: r = mxCreateDoubleScalar(RCHK(x->f)); break;
  
  //char          i.e. "a"
  case -10: r = mxCreateString((char*)&(x->i)); break;
  
  //symbol        i.e. `abc
  case -11: r = mxCreateString(x->s); break;
  
  //timestamp     i.e. dateDtimespan
  case -12: r = mxCreateDoubleScalar(utmtsp(x->j)); break;
  
  //month         i.e. 2000.01m
  case -13: {
    int timy=(x->i)/12+2000;
    int timm=(x->i)%12+1;
    r = mxCreateDoubleMatrix((mwSize)1, (mwSize)2, mxREAL);
    arr = mxGetPr(r);
    arr[0] = timy; arr[1] = timm;  // here we return vector [yyyy mm]
  } break;
    
    //date          i.e. 2000.01.01
  case -14: r = mxCreateDoubleScalar(utmt(x->i)); break;
  
  //datetime  i.e. 2012.01.01T12:12:12.000
  case -15: r = mxCreateDoubleScalar(utmt(x->f)); break;
  
  //timespan      i.e. 0D00:00:00.000000000
  case -16: r = mxCreateDoubleScalar((double)x->j/8.64e13); break;
  
  //minute        i.e. 00:00
  case -17: r = mxCreateDoubleScalar((60.0*(double)x->i)/86400); break;
  
  //second        i.e. 00:00:00
  case -18: r = mxCreateDoubleScalar(((double)x->i)/86400); break;
  
  //time          i.e. 00:00:00.000
  case -19: r = mxCreateDoubleScalar(((double)x->i/1000)/86400); break;
  
  //default
  default: mexPrintf("cannot support %d type\n",x->t); break;
  }
  
  return r;
}



/**
* Process list
*/
mxArray* makelist(K x, int vertical) {
  K obj;
  mxArray* r = NULL;
  int i,j, ktype, nData = x->n, v, matrix_type, matrix_columns, matrix=0;
  mwSize ndims[] = {(mwSize) nData};
  mwSize ndimsc[] = {1,(mwSize) nData};
  mxLogical* ll;
  mwSize rows, cols;
  double* dl;
  mxChar* cl;
  mxInt8* mxInt8s;
  mxInt16* mxInt16s;
  mxInt32* mxInt32s;
  mxInt64* mxInt64s;
  mxSingle* mxFloats;
  
  if (vertical) {rows = nData; cols = 1;ndimsc[0]=nData;ndimsc[1]=1;} else {rows = 1; cols = nData;}  
  ktype = x->t;
  if ((ktype == 0) && (nData>0)) {
    /* First check if all lists  are same length and type */
    matrix_type=kK(x)[0]->t;
    if (matrix_type>=4&&matrix_type<=9&&nData>1) {
      matrix_columns=kK(x)[0]->n;matrix=1;
      for(i=1;i<nData;i++) 
        if ((kK(x)[i]->t != matrix_type) || (kK(x)[0]->n != matrix_columns)) matrix=0;
    }  
    if (matrix) { 
      //mexPrintf("This is a matrix of %d type %d length\n",matrix_type,matrix_columns);
      ktype=matrix_type;
    }    
  }
  
  switch (ktype) {
  //mixed   i.e. 1 1b 0x01
  case 0:
    r = mxCreateCellArray((mwSize) 2, ndimsc);
    for(i=0;i<nData;i++)
      if (kK(x)[i]->t<0) mxSetCell(r, i, makeatom(kK(x)[i]));
      else if (kK(x)[i]->t == 98) mxSetCell(r, i, maketab(kK(x)[i]));
      else if (kK(x)[i]->t == 99) mxSetCell(r, i, makedict(kK(x)[i]));
      else mxSetCell(r, i, makelist(kK(x)[i],0));
      break;
      
      //boolean       i.e. 01011b
  case 1:
    r = mxCreateLogicalArray((mwSize) 2, ndimsc);
    ll = mxGetLogicals(r);
    memcpy(ll, kG(x), nData * sizeof(bool));
    break;
    
    //byte    i.e. 0x0204
  case 4:
    if (matrix) {
      r = mxCreateNumericMatrix((mwSize) nData, matrix_columns, mxINT8_CLASS,mxREAL);
      mxInt8s = mxGetInt8s(r);
      for(i=0;i<matrix_columns;i++)
        for(j=0;j<nData;j++) 
          *(mxInt8s+(i*nData+j))=kG(kK(x)[j])[i];
    }
    else {
      r = mxCreateNumericMatrix(rows, cols, mxINT8_CLASS,mxREAL);
      mxInt8s = mxGetInt8s(r);
      for(i=0;i<nData;i++) *(mxInt8s+i)=kG(x)[i];
    }  
    break;
    
    //short   i.e. 1 2h
  case 5:
    if (matrix) {
      r = mxCreateNumericMatrix((mwSize) nData, matrix_columns, mxINT16_CLASS,mxREAL);
      mxInt16s = mxGetInt16s(r);
      for(i=0;i<matrix_columns;i++)
        for(j=0;j<nData;j++) 
          *(mxInt16s+(i*nData+j))=ICHK(kH(kK(x)[j])[i]);
    }
    else {
      r = mxCreateNumericMatrix(rows, cols, mxINT16_CLASS,mxREAL);
      mxInt16s = mxGetInt16s(r);
      for(i=0;i<nData;i++) *(mxInt16s+i)=ICHK(kH(x)[i]);
    }  
    break;
    
    //int   i.e. 1 2
  case 6:
    if (matrix) {
      r = mxCreateNumericMatrix((mwSize) nData, matrix_columns, mxINT32_CLASS,mxREAL);
      mxInt32s = mxGetInt32s(r);
      for(i=0;i<matrix_columns;i++)
        for(j=0;j<nData;j++) 
          *(mxInt32s+(i*nData+j))=LCHK(kI(kK(x)[j])[i]);
    }
    else {
      r = mxCreateNumericMatrix(rows, cols, mxINT32_CLASS,mxREAL);
      mxInt32s = mxGetInt32s(r);
      for(i=0;i<nData;i++) *(mxInt32s+i)=LCHK(kI(x)[i]);
    }
    break;
    
    //long    i.e. 1 2j
  case 7:
    if (matrix) {
      r = mxCreateNumericMatrix((mwSize) nData, matrix_columns, mxINT64_CLASS,mxREAL);
      mxInt64s = mxGetInt64s(r);
      for(i=0;i<matrix_columns;i++)
        for(j=0;j<nData;j++) 
          *(mxInt64s+(i*nData+j))=LCHK(kJ(kK(x)[j])[i]);
    }
    else {
      r = mxCreateNumericMatrix(rows, cols, mxINT64_CLASS,mxREAL);
      mxInt64s = mxGetInt64s(r);
      for(i=0;i<nData;i++) *(mxInt64s+i)=LCHK(kJ(x)[i]);
    }
    break;
    
    //real    i.e. 1.0 2.0e
  case 8:
    if (matrix) {
      r = mxCreateNumericMatrix((mwSize) nData, matrix_columns, mxSINGLE_CLASS,mxREAL);
      mxFloats = mxGetSingles(r);
      for(i=0;i<matrix_columns;i++)
        for(j=0;j<nData;j++) 
          *(mxFloats+(i*nData+j))=RCHK(kE(kK(x)[j])[i]);
    }
    else {
      r = mxCreateNumericMatrix(rows, cols, mxSINGLE_CLASS,mxREAL);
      mxFloats = mxGetSingles(r);
      for(i=0;i<nData;i++) *(mxFloats+i)=RCHK(kE(x)[i]);
    }  
    break;
    
    //float   i.e. 1.0 2.0
  case 9:
    if (matrix) {
      r = mxCreateNumericMatrix((mwSize) nData, matrix_columns, mxDOUBLE_CLASS,mxREAL);
      dl = mxGetDoubles(r);
      for(i=0;i<matrix_columns;i++)
        for(j=0;j<nData;j++) 
          *(dl+(i*nData+j))=RCHK(kF(kK(x)[j])[i]);
    }
    else {
      r = mxCreateDoubleMatrix(rows, cols, mxREAL);
      dl = mxGetDoubles(r);
      for(i=0;i<nData;i++) dl[i] = RCHK(kF(x)[i]);
    }  
    break;
    
    //char    i.e. "ab"
  case 10:
    r = mxCreateCharArray((mwSize) 2, ndimsc);
    cl = mxGetData(r);
    for(i=0;i<nData;i++) cl[i] = kC(x)[i];
    break;
    
    //symbol  i.e. `a`b
  case 11:
    //r = mxCreateCellArray((mwSize) 1, ndims);
    r = mxCreateCellArray((mwSize) 2, ndimsc);
    for(i=0;i<nData;i++)
      mxSetCell(r, i, mxCreateString(kS(x)[i]));
    break;
    
    //timestamp i.e. dateDtimespan x2
  case 12:
    r = mxCreateDoubleMatrix(rows, cols, mxREAL);
    dl = mxGetPr(r);
    for(i=0;i<nData;i++) dl[i] = utmtsp(kJ(x)[i]);
    break;
    
    //month   i.e. 2010.01 2010.02
  case 13:
    r = mxCreateCellArray((mwSize) 2, ndimsc);
    for(i=0;i<nData;i++) {
      obj = ka(-KM);
      obj->i = kI(x)[i];
      mxSetCell(r, i, makeatom(obj));
    }
    break;
    
    //date    i.e. 2010.01.01 2010.01.02
  case 14:
    r = mxCreateDoubleMatrix(rows, cols, mxREAL);
    dl = mxGetPr(r);
    for(i=0;i<nData;i++) dl[i] = utmt(kI(x)[i]);
    break;
    
    //datetime  i.e. 2010.01.01T12:12:12.000 x2
  case 15:
    r = mxCreateDoubleMatrix(rows, cols, mxREAL);
    dl = mxGetPr(r);
    for(i=0;i<nData;i++) dl[i] = utmt(kF(x)[i]);
    break;
    
    //timespan  i.e. 0D00:00:00.000000000 x2
  case 16:
    r = mxCreateDoubleMatrix(rows, cols, mxREAL);
    dl = mxGetPr(r);
    for(i=0;i<nData;i++) dl[i] = (double)kJ(x)[i]/8.64e13;
    break;
    
    //minute  i.e. 00:00 00:01
  case 17:
    r = mxCreateDoubleMatrix(rows, cols, mxREAL);
    dl = mxGetPr(r);
    for(i=0;i<nData;i++) dl[i] = (60.0*(double)kI(x)[i])/86400;
    break;
    
    //second  i.e. 00:00:00 00:00:01
  case 18:
    r = mxCreateDoubleMatrix(rows, cols, mxREAL);
    dl = mxGetPr(r);
    for(i=0;i<nData;i++) dl[i] = ((double)kI(x)[i])/86400;
    break;
    
    //time    i.e. 00:00:00.111 00:00:00.222
  case 19:
    r = mxCreateDoubleMatrix(rows, cols, mxREAL);
    dl = mxGetPr(r);
    for(i=0;i<nData;i++) dl[i] = ((double)kI(x)[i]/1000)/86400; break;
  case 98:
    r = maketab(x);break;
  case 99:
    r = makedict(x);break;
  case 101:  
    r = mxCreateLogicalScalar(true);
    break;
    
  default: mexPrintf("list: cannot support %d type\n",x->t); break;
  }
  
  return r;
}


/**
* Process table
*/
mxArray* maketab(K x) {
  mxArray* r = NULL;
  int row,col;
  K flip = ktd(x);
  K colName = kK(flip->k)[0];
  K colData = kK(flip->k)[1];
  int nCols = colName->n;
  int nRows = kK(colData)[0]->n;
  mwSize ndims[] = {(mwSize) 1};
  char** fnames = calloc(nCols, sizeof(char*));
  
  for (col=0; col<nCols; col++) {
    fnames[col] = (char*)kS(colName)[col];
  }
  //mexPrintf("working on tab2 \n");
  // create resulting cell array
  r = mxCreateStructArray((mwSize) 1, ndims, (mwSize) nCols, (const char**)fnames);
  
  // destroy names
  free(fnames);
  //mexPrintf("working on tab3 \n");
  // attach columns
  for (col=0; col<nCols; col++) {
    K list = kK(colData)[col];
    mxSetField(r, 0, kS(colName)[col], makelist(list,1));
  }
  //mexPrintf("working on tab4 \n");
  // unrelease flip
  r0(flip);
  
  //Convert to Table
  //mexPrintf("Converting String\n");
  mxArray *struct_class[1], *table_class[1];
  struct_class[0] = r;
  mexCallMATLAB(1, table_class, 1, struct_class, "struct2table");
  r=table_class[0];
  return r;
}

/**
* Process Cells
*/
mxArray* makecell(K x, K dims) {
  K obj;
  mxArray* r = NULL;
  int i,j, ktype, nData = x->n, v, matrix_type, matrix_columns, matrix=0;
  mwSize ndims[] = {(mwSize) nData};
  mwSize ndimsc[] = {1,(mwSize) nData};
  ktype = x->t;
  r = mxCreateCellArray((mwSize) 2, ndimsc);
  for(i=0;i<nData;i++)
    if (kK(x)[i]->t<0) mxSetCell(r, i, makeatom(kK(x)[i]));
    else if (kK(x)[i]->t == 98) mxSetCell(r, i, maketab(kK(x)[i]));
    else if (kK(x)[i]->t == 99) mxSetCell(r, i, makedict(kK(x)[i]));
    else mxSetCell(r, i, makelist(kK(x)[i],0));
    
    return r;
}

/**
* Process dictionary
*/
mxArray* makedict(K x) {
  mxArray* r = NULL;
  K keyName = kK(x)[0];
  int nName = keyName->n, row;
  char** fnames;
  mwSize ndims[] = {(mwSize) 1};
  K keyData = kK(x)[1];
  //mexPrintf("working on dict %d \n", keyData->t);
  
  // if it's keyed dict. handle it as a table
  if (keyData->t==98) return maketab(x);
  
  // allocate memory for keys names
  fnames = calloc(nName, sizeof(char*));
  
  // collect field names (dicr keys)
  for(row=0;row<nName;row++) {
    fnames[row] = (char*)kS(keyName)[row];
  }
  
  // create resulting cell array
  r = mxCreateStructArray((mwSize) 1, ndims, (mwSize)nName, (const char**)fnames);
  
  // simple case like `a`b`c!(1 2 3)
  if ((keyData->t > 0)&&((keyData->t < 20))) {
    for(row=0;row<nName;row++) {
      //mexPrintf("working on field1 %s %d \n", kS(keyName)[row], keyData->t);
      if (keyData->t==10) // special case for char list : `a`b`c!("abc")
        mxSetField(r, 0, kS(keyName)[row], makeatom(kValue(keyData->t, ((char*) &kK(keyData)[0])+row)));
      else if (keyData->t==13 || keyData->t==14 || keyData->t==17|| keyData->t==18 || keyData->t==19) // special case for month list : `a`b`c!(2000.01m; 2000.02m; 2000.03m)
        mxSetField(r, 0, kS(keyName)[row], makeatom(kValue(keyData->t, ((int*) &kK(keyData)[0])+row)));
      else if (keyData->t==0 && kK(keyData)[row]->n > 1 && kK(keyData)[row]->t > 0)
        mxSetField(r, 0, kS(keyName)[row], makelist(kK(keyData)[row],0));
      else // rest cases
        mxSetField(r, 0, kS(keyName)[row], makeatom(kValue(keyData->t, &kK(keyData)[row])));
    }
    
    free(fnames);
    return r;
  }
  //mexPrintf("working on dict2 \n");
  
  // Check if this is a special dictionary for cells
  if (!strcmp(kS(keyName)[0], "type") && (kK(keyData)[0]->t == -11) && !strcmp(kK(keyData)[0]->s,"cell" ) ) {
    //Get Cell Dimensions
    //mexPrintf("working on dict3 \n");
    
    //mexPrintf("detected cell structures key name %s dims %d %d \n", kS(keyName)[0],kH(kK(keyData)[1])[0], kH(kK(keyData)[1])[1]);
    r  = makecell(kK(keyData)[2],kK(keyData)[1]);
  }
  //  else {
  for(row=0;row<nName;row++){
    //mexPrintf("working on dict4 %d \n", row);
    //mexPrintf("working on dict5 %s %d \n", kS(keyName)[row], kK(keyData)[row]->t);
    //mexPrintf("working on field2 %s type %d len %d rowtype %d \n", kS(keyName)[row], keyData->t,kK(keyData)[row]->n,kK(keyData)[row]->t);
    if (kK(keyData)[row]->t>=0 && (kK(keyData)[row]->n > 1 || kK(keyData)[row]->t >= 0) ) { //&& kK(keyData)[row]->t > 0) {
      mxSetField(r, 0, kS(keyName)[row], makelist(kK(keyData)[row],0));
    } else {
      mxSetField(r, 0, kS(keyName)[row], makeatom(kK(keyData)[row]));
    }
  }
  //  }
  free(fnames);
  return r;
}

K convertCharArray(const mxArray *pm) {
  K k_field, k_elem;
  char* incoming;
  int blen;
  const mwSize *dims;
  S sym;
  blen = mxGetNumberOfElements(pm) + 1;
  incoming = mxCalloc(blen, sizeof(char));
  mxGetString(pm, incoming, blen);
  
  dims = mxGetDimensions(pm);
  int number_of_dimensions = mxGetNumberOfDimensions(pm);
  
  int elements_per_page = dims[0] * dims[1];
  
  int total_number_of_pages = 1;
  
  
  for (int d=2; d<number_of_dimensions; d++) {
    total_number_of_pages *= dims[d];
  }
  if (total_number_of_pages*dims[0] == 1) {
    if (symbol_flag) k_field = ks(incoming); else k_field = kp(incoming);
  }
  else {
    if (symbol_flag) k_field=ktn(KS,0); else k_field = knk(0);
    //mexPrintf("Converting to char array2  dim %d %d %d %d \n", number_of_dimensions, total_number_of_pages, dims[0], dims[1]);
    for (int page=0; page < total_number_of_pages; page++) {
      mwSize row;
      /* On each page, walk through each row. */
      for (int row=0; row<dims[0]; row++)  {
        mwSize column;
        mwSize index = (page * elements_per_page) + row;
        char* subbuff = malloc(dims[1]+1);
        //mexPrintf("Converting to char array  row %d \n", row);
        subbuff[dims[1]] = '\0';
        /* Walk along each column in the current row. */
        for (int column=0; column<dims[1]; column++) {
          //mexPrintf("Converting to char array  column %d index %d \n", column, index);
          subbuff[column] = incoming[index];
          index += dims[0];
        }
        if (symbol_flag) {sym=ss(subbuff);js(&k_field,sym);}
        else {k_elem = kp(subbuff);jk(&k_field,k_elem);}
        free(subbuff);
      }
    }
  }
  mxFree((void *)incoming);
  return k_field;
}

K convertString(const mxArray *pm) {
  K k_field;
  char* incoming;
  const mxArray *cell_element_ptr;
  int blen,total_num_of_elements, m;
  
  //mexPrintf("Converting String\n");
  
  blen = mxGetNumberOfElements(pm);
  mxArray *string_class[1], *char_array[1];
  string_class[0] = pm;
  //mexPrintf("Converting String %d\n", blen);
  mexCallMATLAB(1, char_array, 1, string_class, "char");
  //mexPrintf("Converting to char array  \n");
  k_field = convertCharArray(char_array[0]);
  mxDestroyArray(char_array[0]);
  //mexPrintf("Returning k field  \n");
  return k_field;
}


K convertTable(const mxArray *pm) {
  K k_field;
  char* incoming;
  const mxArray *cell_element_ptr;
  int blen,total_num_of_elements, m;
  
  mxArray *table_class[1], *struct_array[1];
  table_class[0] = pm;
  //mexPrintf("Converting String %d\n", blen);
  mexCallMATLAB(1, struct_array, 1, table_class, "table2struct");
  //mexPrintf("Converting to char array  \n");
  k_field = convertStructure(struct_array[0]);
  mxDestroyArray(struct_array[0]);
  //mexPrintf("Returning k field  \n");
  return k_field;
}

K convertStructure(const mxArray *structure_array_ptr){
  int          total_num_of_elements, number_of_fields, index, field_index;
  const char  *field_name;
  const mxArray     *field_array_ptr;
  K k_field, k_val, dict_name, dict_val,k_elem;
  
  //mexPrintf("Converting Structure \n");
  total_num_of_elements = mxGetNumberOfElements(structure_array_ptr); 
  number_of_fields = mxGetNumberOfFields(structure_array_ptr);
  k_field = kp("Not yet implemented");
  if (total_num_of_elements == 1) {
    dict_name=ktn(KS,0);
    dict_val=knk(0);
    /* For the given index, walk through each field. */ 
    for (field_index=0; field_index<number_of_fields; field_index++)  {
      field_name = mxGetFieldNameByNumber(structure_array_ptr, field_index);
      field_array_ptr = mxGetFieldByNumber(structure_array_ptr, index, field_index);
      if (field_array_ptr == NULL) {
        mexPrintf("\tEmpty Field\n");
      } else {
        S sym=ss(field_name);
        js(&dict_name,sym);
        k_val = convertClass(field_array_ptr);
        jk(&dict_val,k_val);
      }
    }
    k_field = xD(dict_name,dict_val);
  } else {
    k_field = knk(0);
    for (index=0; index<total_num_of_elements; index++)  {
      dict_name=ktn(KS,0);
      dict_val=knk(0);
      for (field_index=0; field_index<number_of_fields; field_index++)  {
        field_name = mxGetFieldNameByNumber(structure_array_ptr, field_index);
        field_array_ptr = mxGetFieldByNumber(structure_array_ptr, index, field_index);
        if (field_array_ptr == NULL)
          mexPrintf("\tEmpty Field\n");
        else {
          S sym=ss(field_name);
          js(&dict_name,sym);
          k_val = convertClass(field_array_ptr);
          jk(&dict_val,k_val);
        }
      }
      k_elem = xD(dict_name,dict_val);
      jk(&k_field,k_elem);
      //k_field=k_elem;
    }
    
  }
  
  return k_field;
}

K convertCell(const mxArray *cell_array_ptr)
{
  int       total_num_of_cells;
  int       index;
  const mwSize *dims;
  const mxArray *cell_element_ptr;
  K k_field, k_val, dict_name, dict_val,k_elem, temp;
  total_num_of_cells = mxGetNumberOfElements(cell_array_ptr); 
  dict_name=ktn(KS,0);
  dict_val=knk(0);
  
  /* Each cell mxArray contains m-by-n cells; Each of these cells
  is an mxArray. */ 
  /*k_field = knk(0);
  for (index=0; index<total_num_of_cells; index++)  {
  cell_element_ptr = mxGetCell(cell_array_ptr, index);
  if (cell_element_ptr == NULL) 
  mexPrintf("\tEmpty Cell\n");
  else {
  k_elem = convertClass(cell_element_ptr);
  jk(&k_field,k_elem);
  }
  }*/
  dims = mxGetDimensions(cell_array_ptr);
  
  /*  S sym=ss("type"); js(&dict_name,sym);
  k_val=ks("cell"); jk(&dict_val,k_val);
  
  sym=ss("dims"); js(&dict_name,sym);
  k_field=ktn(KH,0);index=dims[0];ja(&k_field,&index);index=dims[1];ja(&k_field,&index);
  jk(&dict_val,k_field);
  */
  int number_of_dimensions = mxGetNumberOfDimensions(cell_array_ptr);
  int elements_per_page = dims[0] * dims[1];
  
  int total_number_of_pages = 1;
  //   mexPrintf(" dims %d pages %d rows %d cols %d ", number_of_dimensions, total_number_of_pages, dims[0], dims[1]);
  for (int d=2; d<number_of_dimensions; d++) total_number_of_pages *= dims[d];
  if (total_number_of_pages*dims[1]*dims[0] == 1) {
    k_field = knk(0);
    cell_element_ptr = mxGetCell(cell_array_ptr, 0);
    if (cell_element_ptr == NULL) mexPrintf("\tEmpty Cell\n");
    else {
      k_elem = convertClass(cell_element_ptr);
      jk(&k_field,k_elem);
    }
  } 
  else if (total_number_of_pages == 1 && dims[0] == 1) {
    k_field = knk(0);
    for (int index=0; index<dims[1]; index++)  {
      cell_element_ptr = mxGetCell(cell_array_ptr, index);
      if (cell_element_ptr == NULL) mexPrintf("\tEmpty Cell\n");
      else {
        k_elem = convertClass(cell_element_ptr);
        jk(&k_field,k_elem);
      }
    } 
  } else {
    k_field = knk(0);
    //     mexPrintf(" going through pages \n");
    for (int page=0; page < total_number_of_pages; page++) {
      for (int row=0; row<dims[0]; row++)  {
        mwSize index = (page * elements_per_page) + row;
        k_elem=knk(0);
        for (int column=0; column<dims[1]; column++) {
          cell_element_ptr = mxGetCell(cell_array_ptr, index);
          //temp= *(mxInts+index);
          temp = convertClass(cell_element_ptr);
          ja(&k_elem,&temp);
          index += dims[0];
        }
        jk(&k_field,k_elem);
      }
    }
  }
  
  //sym=ss("content"); js(&dict_name,sym);
  //jk(&dict_val,k_field);
  //k_field = xD(dict_name,dict_val);
  return k_field;
}


K convertLogical(const mxArray *pa)
{
  K k_field, k_elem;
  mxLogical* mxLogicals;
  const mwSize *dims;
  char temp;
  if (mxIsComplex(pa)) 
    mexPrintf(" complex numbers not supported "); 
  mxLogicals = mxGetLogicals(pa);
  dims = mxGetDimensions(pa);
  int number_of_dimensions = mxGetNumberOfDimensions(pa);
  int elements_per_page = dims[0] * dims[1];
  
  int total_number_of_pages = 1;
  //   mexPrintf(" dims %d pages %d rows %d cols %d ", number_of_dimensions, total_number_of_pages, dims[0], dims[1]);
  for (int d=2; d<number_of_dimensions; d++) total_number_of_pages *= dims[d];
  if (total_number_of_pages*dims[1]*dims[0] == 1) {
    k_field = kb(*(char *)mxLogicals);
  } 
  else if (total_number_of_pages == 1 && dims[0] == 1) {
    //      mexPrintf(" going through vector \n");
    k_field=ktn(KB,0);
    for (int index=0; index<dims[1]; index++)  {
      temp= *mxLogicals++;
      ja(&k_field,&temp);
    } 
  } else {
    k_field = knk(0);
    //     mexPrintf(" going through pages \n");
    for (int page=0; page < total_number_of_pages; page++) {
      for (int row=0; row<dims[0]; row++)  {
        mwSize index = (page * elements_per_page) + row;
        k_elem=ktn(KB,0);
        for (int column=0; column<dims[1]; column++) {
          temp= *(mxLogicals+index);
          ja(&k_elem,&temp);
          index += dims[0];
        }
        jk(&k_field,k_elem);
      }
    }
  }
  return k_field;
}  /* convertLogical */
  
  
  K convertInt8s(const mxArray *pa, int isSigned)
  {
    K k_field, k_elem;
    mxInt8* mxInts;
    const mwSize *dims;
    char temp;
    if (mxIsComplex(pa)) 
      mexPrintf(" complex numbers not supported "); 
    if (isSigned) mxInts = mxGetInt8s(pa); else mxInts = (mxInt8*)mxGetUint8s(pa); 
    dims = mxGetDimensions(pa);
    int number_of_dimensions = mxGetNumberOfDimensions(pa);
    int elements_per_page = dims[0] * dims[1];
    
    int total_number_of_pages = 1;
    //   mexPrintf(" dims %d pages %d rows %d cols %d ", number_of_dimensions, total_number_of_pages, dims[0], dims[1]);
    for (int d=2; d<number_of_dimensions; d++) total_number_of_pages *= dims[d];
    if (total_number_of_pages*dims[1]*dims[0] == 1) {
      k_field = kg(*(int *)mxInts);
    } 
    else if (total_number_of_pages == 1 && dims[0] == 1) {
      //      mexPrintf(" going through vector \n");
      k_field=ktn(KG,0);
      for (int index=0; index<dims[1]; index++)  {
        temp= *mxInts++;
        ja(&k_field,&temp);
      } 
    } else {
      k_field = knk(0);
      //     mexPrintf(" going through pages \n");
      for (int page=0; page < total_number_of_pages; page++) {
        for (int row=0; row<dims[0]; row++)  {
          mwSize index = (page * elements_per_page) + row;
          k_elem=ktn(KG,0);
          for (int column=0; column<dims[1]; column++) {
            temp= *(mxInts+index);
            ja(&k_elem,&temp);
            index += dims[0];
          }
          jk(&k_field,k_elem);
        }
      }
    }
    return k_field;
  }  /* convertInt8s */
  
  K convertInt16s(const mxArray *pa, int isSigned)
  {
    K k_field, k_elem;
    mxInt16* mxInts;
    const mwSize *dims;
    int temp;
    if (mxIsComplex(pa)) 
      mexPrintf(" complex numbers not supported "); 
    if (isSigned) mxInts = mxGetInt16s(pa); else mxInts = (mxInt16*)mxGetUint16s(pa); 
    dims = mxGetDimensions(pa);
    int number_of_dimensions = mxGetNumberOfDimensions(pa);
    int elements_per_page = dims[0] * dims[1];
    
    int total_number_of_pages = 1;
    //   mexPrintf(" dims %d pages %d rows %d cols %d ", number_of_dimensions, total_number_of_pages, dims[0], dims[1]);
    for (int d=2; d<number_of_dimensions; d++) total_number_of_pages *= dims[d];
    if (total_number_of_pages*dims[1]*dims[0] == 1) {
      k_field = kh(*(int *)mxInts);
    } 
    else if (total_number_of_pages == 1 && dims[0] == 1) {
      //      mexPrintf(" going through vector \n");
      k_field=ktn(KH,0);
      for (int index=0; index<dims[1]; index++)  {
        temp= *mxInts++;
        ja(&k_field,&temp);
      } 
    } else {
      k_field = knk(0);
      //     mexPrintf(" going through pages \n");
      for (int page=0; page < total_number_of_pages; page++) {
        for (int row=0; row<dims[0]; row++)  {
          mwSize index = (page * elements_per_page) + row;
          k_elem=ktn(KH,0);
          for (int column=0; column<dims[1]; column++) {
            temp= *(mxInts+index);
            ja(&k_elem,&temp);
            index += dims[0];
          }
          jk(&k_field,k_elem);
        }
      }
    }
    return k_field;
  }  /* convertInt16s */
  
  K convertInt32s(const mxArray *pa, int isSigned)
  {
    K k_field, k_elem;
    mxInt32* mxInts;
    const mwSize *dims;
    int temp;
    if (mxIsComplex(pa)) 
      mexPrintf(" complex numbers not supported "); 
    if (isSigned) mxInts = mxGetInt32s(pa); else mxInts = (mxInt32*)mxGetUint32s(pa); 
    dims = mxGetDimensions(pa);
    int number_of_dimensions = mxGetNumberOfDimensions(pa);
    int elements_per_page = dims[0] * dims[1];
    
    int total_number_of_pages = 1;
    for (int d=2; d<number_of_dimensions; d++) total_number_of_pages *= dims[d];
    if (total_number_of_pages*dims[1]*dims[0] == 1) {
      k_field = ki(*(int *)mxInts);
    } 
    else if (total_number_of_pages == 1 && dims[0] == 1) {
      k_field=ktn(KI,0);
      for (int index=0; index<dims[1]; index++)  {
        temp= *mxInts++;
        ja(&k_field,&temp);
      } 
    } else {
      k_field = knk(0);
      //    mexPrintf(" dims %d pages %d rows %d cols %d ", number_of_dimensions, total_number_of_pages, dims[0], dims[1]);
      for (int page=0; page < total_number_of_pages; page++) {
        for (int row=0; row<dims[0]; row++)  {
          mwSize index = (page * elements_per_page) + row;
          k_elem=ktn(KI,0);
          for (int column=0; column<dims[1]; column++) {
            temp= *(mxInts+index);
            ja(&k_elem,&temp);
            index += dims[0];
          }
          jk(&k_field,k_elem);
        }
      }
    }
    return k_field;
  } /* convertInt32s */

K convertInt64s(const mxArray *pa, int isSigned)
{
  K k_field, k_elem;
  mxInt64* mxInts;
  const mwSize *dims;
  int64_t temp;
  if (mxIsComplex(pa)) 
    mexPrintf(" complex numbers not supported "); 
  
  if (isSigned) mxInts = mxGetInt64s(pa); else mxInts = (mxInt64*)mxGetUint64s(pa); 
  dims = mxGetDimensions(pa);
  int number_of_dimensions = mxGetNumberOfDimensions(pa);
  int elements_per_page = dims[0] * dims[1];
  
  int total_number_of_pages = 1;
  for (int d=2; d<number_of_dimensions; d++) total_number_of_pages *= dims[d];
  if (total_number_of_pages*dims[1]*dims[0] == 1) {
    k_field = kj(*(int64_t *)mxInts);
  } 
  else if (total_number_of_pages == 1 && dims[0] == 1) {
    k_field=ktn(KJ,0);
    for (int index=0; index<dims[1]; index++)  {
      temp= *mxInts++;
      ja(&k_field,&temp);
    } 
  } else {
    k_field = knk(0);
    //    mexPrintf(" dims %d pages %d rows %d cols %d ", number_of_dimensions, total_number_of_pages, dims[0], dims[1]);
    for (int page=0; page < total_number_of_pages; page++) {
      for (int row=0; row<dims[0]; row++)  {
        mwSize index = (page * elements_per_page) + row;
        k_elem=ktn(KJ,0);
        for (int column=0; column<dims[1]; column++) {
          temp= *(mxInts+index);
          ja(&k_elem,&temp);
          index += dims[0];
        }
        jk(&k_field,k_elem);
      }
    }
  }
  return k_field;
} /* convertInt64s */

K convertSingle(const mxArray *pa)
{
  K k_field, k_elem;
  mxSingle* mxFloats;
  const mwSize *dims;
  float temp;
  if (mxIsComplex(pa)) 
    mexPrintf(" complex numbers not supported "); 
  mxFloats = mxGetSingles(pa);
  dims = mxGetDimensions(pa);
  int number_of_dimensions = mxGetNumberOfDimensions(pa);
  int elements_per_page = dims[0] * dims[1];
  int total_number_of_pages = 1;
  for (int d=2; d<number_of_dimensions; d++) total_number_of_pages *= dims[d];
  if (total_number_of_pages*dims[1]*dims[0] == 1) {
    k_field = ke(*mxFloats);
  } 
  else if (total_number_of_pages == 1 && dims[0] == 1) {
    k_field=ktn(KE,0);
    for (int index=0; index<dims[1]; index++)  {
      temp= *mxFloats++;
      ja(&k_field,&temp);
    } 
  } else {
    k_field = knk(0);
    //    mexPrintf(" dims %d pages %d rows %d cols %d ", number_of_dimensions, total_number_of_pages, dims[0], dims[1]);
    for (int page=0; page < total_number_of_pages; page++) {
      for (int row=0; row<dims[0]; row++)  {
        mwSize index = (page * elements_per_page) + row;
        k_elem=ktn(KE,0);
        for (int column=0; column<dims[1]; column++) {
          temp= *(mxFloats+index);
          ja(&k_elem,&temp);
          index += dims[0];
        }
        jk(&k_field,k_elem);
      }
    }
  }
  return k_field;
} /* convertSingle */

K convertDouble(const mxArray *pa)
{
  K k_field, k_elem;
  mxDouble* mxFloats;
  const mwSize *dims;
  double temp;
  if (mxIsComplex(pa)) 
    mexPrintf(" complex numbers not supported "); 
  mxFloats = mxGetDoubles(pa);
  dims = mxGetDimensions(pa);
  int number_of_dimensions = mxGetNumberOfDimensions(pa);
  int elements_per_page = dims[0] * dims[1];
  int total_number_of_pages = 1;
  for (int d=2; d<number_of_dimensions; d++) total_number_of_pages *= dims[d];
  if (total_number_of_pages*dims[1]*dims[0] == 1) {
    k_field = kf(*mxFloats);
  } 
  else if (total_number_of_pages == 1 && dims[0] == 1) {
    k_field=ktn(KF,0);
    for (int index=0; index<dims[1]; index++)  {
      temp= *mxFloats++;
      ja(&k_field,&temp);
    } 
  } else {
    k_field = knk(0);
    //    mexPrintf(" dims %d pages %d rows %d cols %d ", number_of_dimensions, total_number_of_pages, dims[0], dims[1]);
    for (int page=0; page < total_number_of_pages; page++) {
      for (int row=0; row<dims[0]; row++)  {
        mwSize index = (page * elements_per_page) + row;
        k_elem=ktn(KF,0);
        for (int column=0; column<dims[1]; column++) {
          temp= *(mxFloats+index);
          ja(&k_elem,&temp);
          index += dims[0];
        }
        jk(&k_field,k_elem);
      }
    }
  }
  return k_field;
} /* convertDouble */


K convertFull(const mxArray *numeric_array_ptr)
{
  K k_field;
  mxClassID   category;
  k_field = kp("Not yet implemented2");
  
  category = mxGetClassID(numeric_array_ptr);
  //mexPrintf("Converting numeric Type %d \n", category);
  
  switch (category)  {
  case mxINT8_CLASS:   k_field=convertInt8s(numeric_array_ptr,1);   break; 
  case mxUINT8_CLASS:  k_field=convertInt8s(numeric_array_ptr,0); break;
  case mxINT16_CLASS:  k_field=convertInt16s(numeric_array_ptr,1);   break;
  case mxUINT16_CLASS: k_field=convertInt16s(numeric_array_ptr,0); break;
  case mxINT32_CLASS:  k_field=convertInt32s(numeric_array_ptr,1);  break;
  case mxUINT32_CLASS:  k_field=convertInt32s(numeric_array_ptr,0); break;
  case mxINT64_CLASS:  k_field=convertInt64s(numeric_array_ptr,1);  break;
  case mxUINT64_CLASS:  k_field=convertInt64s(numeric_array_ptr,0); break;
  case mxSINGLE_CLASS: k_field=convertSingle(numeric_array_ptr); break; 
  case mxDOUBLE_CLASS: k_field=convertDouble(numeric_array_ptr); break; 
  }
  return k_field;
}

K convertDatetime(const mxArray *pa)
{
  K k_field;
  mxDouble* mxFloats;
  mxInt32* mxInts;
  
  int index, total_num_of_elements;
  double temp;
  k_field = kp("Not yet implemented2");
  
  mxArray *string_class[1], *char_array[1];
  string_class[0] = pa;
  mexCallMATLAB(1, char_array, 1, string_class, "datenum");
  total_num_of_elements = mxGetNumberOfElements(char_array[0]);
  mxFloats = mxGetDoubles(char_array[0]);
  
  if (total_num_of_elements==1) {
    *mxFloats = *mxFloats - 730486;
    k_field = kz(*mxFloats);
  } else {
    k_field=ktn(KZ,0);
    for (index=0; index<total_num_of_elements; index++)  {
      temp= *(mxFloats++) - 730486;
      ja(&k_field,&temp);
    } 
  }
  
  return k_field;
}

K convertClass(const mxArray *pm) {
  mxClassID mxClass;
  char * className;
  K k_field;
  mxClass = mxGetClassID(pm);
  className= mxGetClassName(pm);
  //mexPrintf("Converting type  %d %s \n", mxClass, className);
  if (!strcmp (className, "datetime") )
    mxClass=-999;
  if (!strcmp (className, "table") ) {
    mxClass=-998;
  }    
  k_field = kp("Not yet implemented1");
  if (mxIsSparse(pm)) {
    //analyze_sparse(array_ptr);
    return k_field;
  } else {
    switch (mxClass) {
    case mxCHAR_CLASS:    return convertCharArray(pm);     break;
    case mxLOGICAL_CLASS: return convertLogical(pm); break;
    case 19:    return convertString(pm);     break;
    case -999:   return convertDatetime(pm);  break;
    case -998:   return convertTable(pm); break;
    case mxSTRUCT_CLASS:  return convertStructure(pm);  break;
    case mxCELL_CLASS:    return convertCell(pm);       break;
    case mxUNKNOWN_CLASS: mexWarnMsgTxt("Unknown class."); break;
    default:              return convertFull(pm);       break;
    }
  }
  return k_field;
  
}
/**
* Function entry qdbc
*/
void mexFunction(int nlhs, mxArray* pout[],
                 int nrhs, const mxArray* pin[]) {
  K arg_item, args[10], res;
  char buffer[1024];
  char* query;
  char* host;
  char* user;
  I c;
  int blen, port, index, sync_flag,error=0;
  mxArray* tmp;
  
  // - check args
  if (nrhs < 3)
    mexErrMsgIdAndTxt(MATID, "Two inputs required.");
  else if(!mxIsStruct(pin[0]))
    mexErrMsgIdAndTxt("qdbc:inputNotStruct", "Input must be a structure.");
  
  // - get sync flag
  sync_flag = *((int *)mxGetUint8s(pin[1]));
  
  // - get sybmbol flag
  symbol_flag = *((int *)mxGetUint8s(pin[2]));
  
  if (sync_flag == 99) {
    int * crash = NULL;
    *crash = 1;
  }
  // - get query
  blen = mxGetNumberOfElements(pin[3]) + 1;
  if (blen==1)
    mexErrMsgIdAndTxt("qdbc:emptyQuery", "Query can't be empty");
  query = mxCalloc(blen, sizeof(char));
  mxGetString(pin[3], query, blen);
  
  
  // - get host
  tmp = mxGetField(pin[0], 0, "host");
  if (tmp==NULL)
    mexErrMsgIdAndTxt("qdbc:FieldNotSpecified", "Can't find 'host' field in structure");
  blen = mxGetNumberOfElements(tmp) + 1;
  host = mxCalloc(blen, sizeof(char));
  mxGetString(tmp, host, blen);
  
  // - get port
  tmp = mxGetField(pin[0], 0, "port");
  if (tmp==NULL)
    mexErrMsgIdAndTxt("qdbc:FieldNotSpecified", "Can't find 'port' field in structure");
  port = (int) mxGetScalar(tmp);
  
  // - get user
  tmp = mxGetField(pin[0], 0, "user");
  if (tmp==NULL)
    mexErrMsgIdAndTxt("qdbc:FieldNotSpecified", "Can't find 'user' field in structure");
  blen = mxGetNumberOfElements(tmp) + 1;
  user = mxCalloc(blen, sizeof(char));
  mxGetString(tmp, user, blen);
  
  
  //Get extra parameter
  if (nrhs > 4) {
    for (index=4; (index<nrhs && index<14); index++)  {
      arg_item = convertClass(pin[index]);
      args[index-4]=arg_item;
    }   
  }
  //mexPrintf("Number of arguments right %d query %s \n", nrhs, query);
  //mexPrintf("Number of arguments left %d \n", nlhs);
  
  // - connect
  int try=0, wait=10;
  while (try<10) {
     try++;
     c = khpu(host, port, user);
     if (c >= 0) {break;}
     mexPrintf("Retrying connect to DB %d try %d wait \n", c, try, wait);
     sleep(wait);
     wait=wait+10;
  }
  //if (fcntl(c, F_GETFL) > 0 && errno != EBADF) {
  if (c > 0) {
    if (sync_flag==0) c=-1*c;
    // - exec query
    switch (nrhs)  {
    case 4: res = k(c, query, (K) 0); break;
    case 5: res = k(c, query, args[0], (K) 0); break;
    case 6: res = k(c, query, args[0], args[1], (K) 0); break;
    case 7: res = k(c, query, args[0], args[1], args[2], (K) 0); break;
    case 8: res = k(c, query, args[0], args[1], args[2], args[3], (K) 0); break;
    case 9: res = k(c, query, args[0], args[1], args[2], args[3], args[4], (K) 0); break;
    case 10: res = k(c, query, args[0], args[1], args[2], args[3], args[4], args[5], (K) 0); break;
    case 11: res = k(c, query, args[0], args[1], args[2], args[3], args[4], args[5], args[6], (K) 0); break;
    case 12: res = k(c, query, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], (K) 0); break;
    case 13: res = k(c, query, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], (K) 0); break;
    case 14: res = k(c, query, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], (K) 0); break;
    case 15: res = k(c, query, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10], (K) 0); break;
    case 16: res = k(c, query, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11], (K) 0); break;
    }
    
    if (res) {
      //    mexPrintf("%s>> type %d\n", MATID, res->t);
      if (res->t != -128) {
        if (res->t < 0) pout[0] = makeatom(res);
        else if (res->t >= 0 && res->t <= 19) pout[0] = makelist(res,0);
        else if (res->t == 98) pout[0] = maketab(res);
        else if (res->t == 99) pout[0] = makedict(res);
        else if (res->t == 101) { pout[0] = mxCreateLogicalScalar(true); mexPrintf("::\n");} // when void query result return t
        else {pout[0] = mxCreateString("Error: Type not supported");error = 1;}
      } else {
        strcpy(buffer, "Error: ");strcat(buffer,res->s);
        pout[0] = mxCreateString(buffer);
        error = 1;
      }
    } else {
      mexPrintf("Error here  \n");
      pout[0] = mxCreateString("Can't get query result"); error = 1;
    }
    
    if ((sync_flag==2) && (!error)) { //Block and wait for message back if the function did not error
      //       mexPrintf("Making a blocking call\n");
      res = k(c,(S)0); 
      if (res) {
        //mexPrintf("%s>> type %d\n", MATID, res->t);
        if (res->t != -128) {
          if (res->t < 0) pout[0] = makeatom(res);
          else if (res->t >= 0 && res->t <= 19) pout[0] = makelist(res,0);
          else if (res->t == 98) pout[0] = maketab(res);
          else if (res->t == 99) pout[0] = makedict(res);
          else if (res->t == 101) { pout[0] = mxCreateLogicalScalar(true); mexPrintf("::\n");} // when void query result return t
          else pout[0] = mxCreateString("Type not supported");       //mexPrintf("%s >> Type %d is not supported\n", MATID, res->t);
        } else {
          strcpy(buffer, "Error: ");strcat(buffer,res->s);
          pout[0] = mxCreateString(buffer);
          error = 1;
        }
      } else {
        pout[0] = mxCreateString("Can't get query result"); 
      }
    }
    
    k(c,"",(K)0);
    kclose(c);
  } else if (c == 0) {
    mexPrintf("Host %s port %d Invalid password \n", host, port);
    mexErrMsgIdAndTxt("qdbc:PasswordIncorrect", "Incorrect password specified");
  }
  else {
    mexPrintf("Host %s port %d Descriptor is invalid : %d\n", host, port, c);
    mexErrMsgIdAndTxt("qdbc:UnableToConnect", "Cannot connect to host and port");
  }
  mxFree(query);
  mxFree(host);
}
