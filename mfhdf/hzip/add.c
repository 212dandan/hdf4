/****************************************************************************
 * NCSA HDF                                                                 *
 * Software Development Group                                               *
 * National Center for Supercomputing Applications                          *
 * University of Illinois at Urbana-Champaign                               *
 * 605 E. Springfield, Champaign IL 61820                                   *
 *                                                                          *
 * For conditions of distribution and use, see the accompanying             *
 * hdf/COPYING file.                                                        *
 *                                                                          *
 ****************************************************************************/


#include "hdf.h"
#include "mfhdf.h"
#include "hzip.h"
#include "pal_rgb.h"
#include "add.h"



/* read image data */
int      X_LENGTH;
int      Y_LENGTH;
int      N_COMPS;
unsigned char *image_data = 0;   

/* sds */
#define X_DIM      4
#define Y_DIM      16


/*-------------------------------------------------------------------------
 * Function: add_gr
 *
 * Purpose: utility function to read an image data file and save the image with the
 *  GR - Multifile General Raster Image Interface,
 *  optionally inserting the image into the group VGROUP_ID
 *
 * Return: void
 *
 * Programmer: Pedro Vicente, pvn@ncsa.uiuc.edu
 *
 * Date: July 3, 2003
 *
 *-------------------------------------------------------------------------
 */

void add_gr(char* name_file,char* gr_name,int32 file_id,int32 vgroup_id)
{
 intn   status_n;       /* returned status_n for functions returning an intn  */
 int32  status_32,      /* returned status_n for functions returning an int32 */
        gr_id,          /* GR interface identifier */
        ri_id,          /* raster image identifier */
        gr_ref,         /* reference number of the GR image */
        start[2],       /* start position to write for each dimension */
        edges[2],       /* number of elements to be written along each dimension */
        dim_gr[2],      /* dimension sizes of the image array */
        interlace_mode, /* interlace mode of the image */
        data_type;      /* data type of the image data */
 char   *srcdir = getenv("srcdir"); /* the source directory */
 char   data_file[512]="";          /* buffer to hold name of existing data file */
 uint8  attr_values[2]={1,2};
 int    n_values;
 
 /* compose the name of the file to open, using the srcdir, if appropriate */
 if ( srcdir )
 {
  strcpy(data_file, srcdir);
  strcat(data_file, "/");
 }
 strcat( data_file, name_file);
 if ( read_data(data_file)>0)
 {
  /* set the data type, interlace mode, and dimensions of the image */
  data_type = DFNT_UINT8;
  interlace_mode = MFGR_INTERLACE_PIXEL;
  dim_gr[0] = X_LENGTH;
  dim_gr[1] = Y_LENGTH;

  /* initialize the GR interface */
  gr_id = GRstart (file_id);
  
  /* create the raster image array */
  ri_id = GRcreate (gr_id, gr_name, N_COMPS, data_type, interlace_mode, dim_gr);
  
  /* define the size of the data to be written */
  start[0] = start[1] = 0;
  edges[0] = X_LENGTH;
  edges[1] = Y_LENGTH;
  
  /* write the data in the buffer into the image array */
  status_n = GRwriteimage(ri_id, start, NULL, edges, (VOIDP)image_data);

  /* assign an attribute to the SDS */
  n_values = 2;
  status_n = GRsetattr(ri_id, "Myattr", DFNT_UINT8, n_values, (VOIDP)attr_values);
  
  /* obtain the reference number of the GR using its identifier */
  gr_ref = GRidtoref (ri_id);


#if defined( HZIP_DEBUG)
  printf("add_gr %d\n",gr_ref); 
#endif
  
  /* add the GR to the vgroup. the tag DFTAG_RIG is used */
  if (vgroup_id)
   status_32 = Vaddtagref (vgroup_id, TAG_GRP_IMAGE, gr_ref);

  /* terminate access to the raster image */
  status_n = GRendaccess (ri_id);

  /* terminate access to the GR interface */
  status_n = GRend (gr_id);

 }

 if ( image_data )
 {
  free( image_data );
  image_data=NULL;
 }

}


/*-------------------------------------------------------------------------
 * Function: add_r8
 *
 * Purpose: utility function to read an image data file and save the image with the
 *  DFR8 - Single-file 8-Bit Raster Image Interface,
 *  optionally inserting the image into the group VGROUP_ID
 *
 * Return: void
 *
 * Programmer: Pedro Vicente, pvn@ncsa.uiuc.edu
 *
 * Date: July 3, 2003
 *
 *-------------------------------------------------------------------------
 */

void add_r8(char *fname,char* name_file,int32 vgroup_id)
{
 intn   status_n;       /* returned status_n for functions returning an intn  */
 int32  status_32,      /* returned status_n for functions returning an int32 */
        ri_ref;         /* reference number of the GR image */
 char   *srcdir = getenv("srcdir"); /* the source directory */
 char   data_file[512]="";          /* buffer to hold name of existing data file */
 
 /* compose the name of the file to open, using the srcdir, if appropriate */
 if ( srcdir )
 {
  strcpy(data_file, srcdir);
  strcat(data_file, "/");
 }
 strcat( data_file, name_file);
 if ( read_data(data_file)>0)
 {
  /* add a palette */
  status_n = DFR8setpalette(pal_rgb);

  /* write the image */
  status_n = DFR8addimage(fname, image_data, X_LENGTH, Y_LENGTH, 0);
  
  /* obtain the reference number of the RIS8 */
  ri_ref = DFR8lastref();

#if defined( HZIP_DEBUG)
  printf("add_r8 %d\n",ri_ref); 
#endif
  
  /* add the image to the vgroup. the tag DFTAG_RIG is used */
  if (vgroup_id)
   status_32 = Vaddtagref (vgroup_id, TAG_GRP_IMAGE, ri_ref);
 }

 if ( image_data )
 {
  free( image_data );
  image_data=NULL;
 }

}


/*-------------------------------------------------------------------------
 * Function: add_r24
 *
 * Purpose: utility function to read an image data file and save the image with the
 *  DF24 - Single-file 24-Bit Raster Image Interface,
 *  optionally inserting the image into the group VGROUP_ID
 *
 * Return: void
 *
 * Programmer: Pedro Vicente, pvn@ncsa.uiuc.edu
 *
 * Date: July 3, 2003
 *
 *-------------------------------------------------------------------------
 */

void add_r24(char *fname,char* name_file,int32 vgroup_id)
{
 intn   status_n;       /* returned status_n for functions returning an intn  */
 int32  status_32,      /* returned status_n for functions returning an int32 */
        ri_ref;         /* reference number of the GR image */
 char   *srcdir = getenv("srcdir"); /* the source directory */
 char   data_file[512]="";          /* buffer to hold name of existing data file */
 
 /* compose the name of the file to open, using the srcdir, if appropriate */
 if ( srcdir )
 {
  strcpy(data_file, srcdir);
  strcat(data_file, "/");
 }
 strcat( data_file, name_file);
 if ( read_data(data_file)>0)
 {
  /* set pixel interlace */
  status_n = DF24setil(DFIL_PIXEL);

  /* write the image */
  status_n = DF24addimage(fname, image_data, X_LENGTH, Y_LENGTH);
  
  /* obtain the reference number of the RIS24 */
  ri_ref = DF24lastref();

#if defined( HZIP_DEBUG)
  printf("add_r24 %d\n",ri_ref); 
#endif
  
  /* add the image to the vgroup. the tag DFTAG_RIG is used */
  if (vgroup_id)
   status_32 = Vaddtagref (vgroup_id, TAG_GRP_IMAGE, ri_ref);
 }

 if ( image_data )
 {
  free( image_data );
  image_data=NULL;
 }

}



/*-------------------------------------------------------------------------
 * Function: add_sd
 *
 * Purpose: utility function to write with
 *  SD - Multifile Scientific Data Interface,
 *  optionally :
 *   1)inserting the SD into the group VGROUP_ID
 *   2)making the dataset chunked and/or compressed
 *
 * Return: void
 *
 * Programmer: Pedro Vicente, pvn@ncsa.uiuc.edu
 *
 * Date: July 3, 2003
 *
 *-------------------------------------------------------------------------
 */

void add_sd(char *fname,             /* file name */
            char* sds_name,          /* sds name */
            int32 vgroup_id,         /* group ID */
            int32 chunk_flags,       /* chunk flags */
            int32 comp_type,         /* compression flag */
            comp_info *comp_info     /* compression structure */ )

{
 intn   status_n;     /* returned status_n for functions returning an intn  */
 int32  status_32,    /* returned status_n for functions returning an int32 */
        sd_id,        /* SD interface identifier */
        sds_id,       /* data set identifier */
        sds_ref,      /* reference number of the data set */
        dim_sds[2],   /* dimension of the data set */
        rank = 2,     /* rank of the data set array */
        n_values,     /* number of values of attribute */
        dim_index,    /* dimension index */
        dim_id,       /* dimension ID */
        start[2],     /* write start */
        edges[2],     /* write edges */
        fill_value=2, /* fill value */
        data[Y_DIM][X_DIM];
 float32 sds_values[2] = {2., 10.}; /* values of the SDS attribute  */
 int16   data_X[X_DIM];             /* X dimension dimension scale */
 float64 data_Y[Y_DIM];             /* Y dimension dimension scale */
 int     i, j;
 HDF_CHUNK_DEF chunk_def;           /* Chunking definitions */ 

 
 /* Define chunk's dimensions */
 chunk_def.chunk_lengths[0] = Y_DIM/2;
 chunk_def.chunk_lengths[1] = X_DIM/2;
 /* To use chunking with RLE, Skipping Huffman, and GZIP compression */
 chunk_def.comp.chunk_lengths[0] = Y_DIM/2;
 chunk_def.comp.chunk_lengths[1] = X_DIM/2;
 
 /* GZIP compression, set compression type, flag and deflate level*/
 chunk_def.comp.comp_type = COMP_CODE_DEFLATE;
 chunk_def.comp.cinfo.deflate.level = 6;             

 /* data set data initialization */
 for (j = 0; j < Y_DIM; j++) {
  for (i = 0; i < X_DIM; i++)
   data[j][i] = (i + j) + 1;
 }
/* initialize dimension scales */
 for (i=0; i < X_DIM; i++) data_X[i] = i;
 for (i=0; i < Y_DIM; i++) data_Y[i] = 0.1 * i;


 /* initialize the SD interface */
 sd_id = SDstart (fname, DFACC_WRITE);
 
 /* set the size of the SDS's dimension */
 dim_sds[0] = Y_DIM;
 dim_sds[1] = X_DIM;
 
 /* create the SDS */
 sds_id = SDcreate (sd_id, sds_name, DFNT_INT32, rank, dim_sds);

 /* set chunk */
 if ( (chunk_flags == HDF_CHUNK) || (chunk_flags == (HDF_CHUNK | HDF_COMP)) )
  status_n = SDsetchunk (sds_id, chunk_def, chunk_flags);
 
 /* use compress without chunk-in */
 else if ( (chunk_flags==HDF_NONE || chunk_flags==HDF_CHUNK) && 
            comp_type>COMP_CODE_NONE && comp_type<COMP_CODE_INVALID)
  status_n = SDsetcompress (sds_id, comp_type, comp_info); 

 /* set a fill value */
 status_n = SDsetfillvalue (sds_id, (VOIDP)&fill_value);
  
 /* define the location and size of the data to be written to the data set */
 start[0] = 0;
 start[1] = 0;
 edges[0] = Y_DIM;
 edges[1] = X_DIM;
 
 /* write the stored data to the data set */
 status_n = SDwritedata (sds_id, start, NULL, edges, (VOIDP)data);

/* assign an attribute to the SDS */
 n_values = 2;
 status_n = SDsetattr (sds_id, "Valid_range", DFNT_FLOAT32, n_values, (VOIDP)sds_values);
   
/*  For each dimension of the data set specified in SDS_NAME,
 *  get its dimension identifier and set dimension name
 *  and dimension scale. Note that data type of dimension scale 
 *  can be different between dimensions and can be different from 
 *  SDS data type.
 */
 for (dim_index = 0; dim_index < rank; dim_index++) 
 {
 /* select the dimension at position dim_index */
  dim_id = SDgetdimid (sds_id, dim_index);
  
 /* assign name and dimension scale to selected dimension */
  switch (dim_index)
  {
  case 0: 
   status_n = SDsetdimname (dim_id, "Y_Axis");
   n_values = Y_DIM;
   status_n = SDsetdimscale (dim_id,n_values,DFNT_FLOAT64,(VOIDP)data_Y);  
   status_n = SDsetattr (dim_id, "info", DFNT_CHAR8, 7,"meters");
   break;
  case 1: 
   status_n = SDsetdimname (dim_id, "X_Axis");
   n_values = X_DIM; 
   status_n = SDsetdimscale (dim_id,n_values,DFNT_INT16,(VOIDP)data_X);
   status_n = SDsetattr (dim_id, "info", DFNT_CHAR8, 5,"feet");
   break;
  default: 
   break;
  }

 }

 /* obtain the reference number of the SDS using its identifier */
 sds_ref = SDidtoref (sds_id);
 
#if defined( HZIP_DEBUG)
 printf("add_sd %d\n",sds_ref); 
#endif
 
 /* add the SDS to the vgroup. the tag DFTAG_NDG is used */
 if (vgroup_id)
  status_32 = Vaddtagref (vgroup_id, TAG_GRP_DSET, sds_ref);
 
 /* terminate access to the SDS */
 status_n = SDendaccess (sds_id);
 
 /* terminate access to the SD interface */
 status_n = SDend (sd_id);
}


/*-------------------------------------------------------------------------
 * Function: add_vs
 *
 * Purpose: utility function to write with
 *  VS - Vdata Interface,
 *  optionally inserting the VS into the group VGROUP_ID
 *
 * Return: void
 *
 * Programmer: Pedro Vicente, pvn@ncsa.uiuc.edu
 *
 * Date: July 3, 2003
 *
 *-------------------------------------------------------------------------
 */

void add_vs(char* vs_name,int32 file_id,int32 vgroup_id)
{
 intn   status_n;       /* returned status_n for functions returning an intn  */
 int32  status_32,      /* returned status_n for functions returning an int32 */
        vdata_ref;      /* reference number of the vdata */
 char8 vdata1_buf1 [5] = {'V', 'D', 'A', 'T', 'A'};
       
 /* Initialize the VS interface */
 status_n = Vstart (file_id);
 
 /* Create the first vdata and populate it with data from vdata1_buf */
 vdata_ref = VHstoredata (file_id, "MyField", (uint8 *)vdata1_buf1, 
  5, DFNT_CHAR8, vs_name, "MyClass"); 

 
#if defined( HZIP_DEBUG)
 printf("add_vs %d\n",vdata_ref); 
#endif

  /* add the VS to the vgroup. the tag DFTAG_VS is used */
 if (vgroup_id)
  status_32 = Vaddtagref (vgroup_id, DFTAG_VS, vdata_ref);

 /* Terminate access to the VS interface */
 status_n = Vend (file_id);

}


/*-------------------------------------------------------------------------
 * read_data
 * utility function to read ASCII image data
 * the files have a header of the type
 *
 *   components
 *   n
 *   height
 *   n
 *   width
 *   n
 * 
 * followed by the image data
 *
 *-------------------------------------------------------------------------
 */

int read_data(char* file_name)
{
 int    i, n;
 int    color_planes;
 char   str[20];
 FILE   *f;
 int    w, h;

 f = fopen( file_name, "r");

 if ( f == NULL )
 {
  printf( "Could not open file %s. Try set $srcdir \n", file_name );
  return -1;
 }

 fscanf( f, "%s", str );
 fscanf( f, "%d", &color_planes );
 fscanf( f, "%s", str );
 fscanf( f, "%d", &h); 
 fscanf( f, "%s", str );
 fscanf( f, "%d", &w); 

 /* globals */
 N_COMPS=color_planes;
 Y_LENGTH=h;
 X_LENGTH=w;

 if ( image_data )
 {
  free( image_data );
  image_data=NULL;
 }

 image_data = (unsigned char*)malloc(w*h*color_planes*sizeof(unsigned char));

 for (i = 0; i < h*w*color_planes ; i++)
 {
  fscanf( f, "%d",&n );
  image_data[i] = (unsigned char)n;
 }
 fclose(f);

 return 1;

}
