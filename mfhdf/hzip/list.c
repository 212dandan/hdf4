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
#include "copy.h"
#include "parse.h"


void list_vg (char* infname,char* outfname,int32 infile_id,int32 outfile_id,table_t *table,options_t *options);
void list_gr (char* infname,char* outfname,int32 infile_id,int32 outfile_id,table_t *table,options_t *options);
void list_sds(char* infname,char* outfname,int32 infile_id,table_t *table,options_t *options);
void list_vs (char* infname,char* outfname,int32 infile_id,int32 outfile_id,table_t *table,options_t *options);
void vgroup_insert(char* infname, char* outfname, 
                   int32 infile_id, int32 outfile_id,
                   int32 vgroup_id_out, char*path_name, 
                   int32* tags, int32* refs, int npairs, 
                   table_t *table, options_t *options);
int copy_vgroup_attrs(int32 vg_in, int32 vg_out);





/*-------------------------------------------------------------------------
 * Function: list
 *
 * Purpose: locate all HDF objects in the file and compress them using options
 *
 * Return: 0, ok, -1 no
 *
 * Programmer: Pedro Vicente, pvn@ncsa.uiuc.edu
 *
 * Date: July 10, 2003
 *
 * Description:
 *
 * A main loop is used to locate all the objects in the file. This loop preserves the 
 * hierarchy of the file. The algorithm used is 
 * 1) Obtain the number of lone VGroups in the HDF file. 
 * 2) Do a loop for each one of these groups. In each iteration a table is updated 
 *    with the tag/reference pair of an object. 
 *    2.1) Obtain the pairs of tag/references for the group 
 *    2.2) Switch between the tag of the current object. Four cases are possible: 
 *         1) Object is a group: recursively repeat the process (obtain the pairs of
 *            tag/references for this group and do another tag switch). 
 *            Add the object to the table. 
 *         2) Object is a dataset: Add the object to the table.
 *         3) Object is an image: Add the object to the table.  
 *         4) Object is a vdata: Add the object to the table. 
 * 3) Read all the HDF interfaces (SDS, GR and VS), checking for objects that are 
 *    already in the table (meaning they belong to a previous inspected group, 
 *    and should not be added).  These objects belong to a root group. 
 * 4) Read all global attributes and annotations. 
 *
 *-------------------------------------------------------------------------
 */


int list(char* infname, char* outfname, options_t *options)
{
 table_t  *table=NULL;
 int32    infile_id;
 int32    outfile_id;
 intn     status_n;  
 int      i, j;
 char*    err;

 /* init table */
 table_init(&table);

 /* open the input file for read and create the output file */
 infile_id  = Hopen (infname,DFACC_READ,0);
 outfile_id = Hopen (outfname,DFACC_CREATE,0);

 if (infile_id==FAIL || outfile_id==FAIL)
 {
  table_free(table);
  printf("Cannot open files %s and %s\n",infname,outfname);
  return FAIL;
 }

 if (options->verbose && options->trip==0)
  printf("Building list of objects in %s...\n",infname);

 /* iterate tru HDF interfaces */
 list_vg (infname,outfname,infile_id,outfile_id,table,options);
 list_gr (infname,outfname,infile_id,outfile_id,table,options);
 list_sds(infname,outfname,infile_id,table,options);
 list_vs (infname,outfname,infile_id,outfile_id,table,options);

 /* close the HDF files */
 status_n = Hclose (infile_id);
 status_n = Hclose (outfile_id);



#if !defined (ONE_TABLE)

 /* 
 check for objects to compress/uncompress:
 1) the input object names are present in the file
 2) they are valid objects (SDS or GR)
 check only if selected objects are given
 */
 if ( options->trip==0 && options->compress==SELECTED ) 
 {
  if (options->verbose)
   printf("Searching for objects to modify...\n");
  if (options->cp_tbl) {
   for ( i = 0; i < options->cp_tbl->nelems; i++) 
   {
    for ( j = 0; j < options->cp_tbl->objs[i].n_objs; j++) 
    {
     char* obj_name=options->cp_tbl->objs[i].obj_list[j].obj;
     if (options->verbose)
      printf("\t%s",obj_name);

     /* the input object names are present in the file and are valid */
     err=table_check(table,obj_name);
     if (err!=NULL)
     {
      printf("\nError: %s %s in file %s. Exiting...\n",obj_name,err,infname);
      table_free(table);
      return FAIL;
     }
     if (options->verbose)
      printf("...Found\n");
    }
   }
  }
 }


 /* 
 check for objects to chunk:
 1) the input object names are present in the file
 2) they are valid objects (SDS or GR)
 check only if selected objects are given
 */
 if ( options->trip==0 && options->chunk==SELECTED ) 
 {
  if (options->verbose)
   printf("Searching for objects to modify...\n");
  if (options->ck_tbl) {
   for ( i = 0; i < options->ck_tbl->nelems; i++) 
   {
    for ( j = 0; j < options->ck_tbl->objs[i].n_objs; j++) 
    {
     char* obj_name=options->ck_tbl->objs[i].obj_list[j].obj;
     if (options->verbose)
      printf("\t%s",obj_name);

     /* the input object names are present in the file and are valid */
     err=table_check(table,obj_name);
     if (err!=NULL)
     {
      printf("\nError: %s %s in file %s. Exiting...\n",obj_name,err,infname);
      table_free(table);
      exit(1);
     }
     if (options->verbose)
      printf("...Found\n");
    }
   }
  }
 }

#else



 /* 
 check for objects in the file table:
 1) the input object names are present in the file
 2) they are valid objects (SDS or GR)
 check only if selected objects are given (all==0)
 */
 if ( options->trip==0 /*&& (options->all_chunk==0 || && options->all_comp==0) */) 
 {
  if (options->verbose)
   printf("Searching for objects to modify...\n");
   
  for ( i = 0; i < options->op_tbl->nelems; i++) 
  {
   char* obj_name=options->op_tbl->objs[i].path;
   if (options->verbose)
    printf(PFORMAT1,"","",obj_name);
   
   /* the input object names are present in the file and are valid */
   err=table_check(table,obj_name);
   if (err!=NULL)
   {
    printf("\nError: %s %s in file %s. Exiting...\n",obj_name,err,infname);
    table_free(table);
    exit(1);
   }
   if (options->verbose)
    printf("...Found\n");
  }
 }




#endif

 /* free table */
 table_free(table);
 return 0;
}



/*-------------------------------------------------------------------------
 * Function: list_vg
 *
 * Purpose: locate all lone Vgroups in the file
 *
 * Return: void
 *
 *-------------------------------------------------------------------------
 */


void list_vg(char* infname,char* outfname,int32 infile_id,int32 outfile_id,
             table_t *table,options_t *options)
{
 intn  status_n;       /* returned status_n for functions returning an intn  */
 int32 status_32,      /* returned status_n for functions returning an int32 */
       vgroup_ref=-1,  /* reference number of the group */
       vgroup_id,      /* vgroup identifier */
       nlones = 0,     /* number of lone vgroups */
       ntagrefs,       /* number of tag/ref pairs in a vgroup */
       *ref_array=NULL,/* buffer to hold the ref numbers of lone vgroups   */
       *tags,          /* buffer to hold the tag numbers of vgroups   */
       *refs,          /* buffer to hold the ref numbers of vgroups   */
       vgroup_id_out;  /* vgroup identifier */
 char  vgroup_name[VGNAMELENMAX], vgroup_class[VGNAMELENMAX];
 int   i;

 /* initialize the V interface for both files */
 status_n = Vstart (infile_id);
 status_n = Vstart (outfile_id);

/*
 * get and print the names and class names of all the lone vgroups.
 * first, call Vlone with nlones set to 0 to get the number of
 * lone vgroups in the file, but not to get their reference numbers.
 */
 nlones = Vlone (infile_id, NULL, nlones );

 if (nlones > 0)
 {
 /*
  * use the nlones returned to allocate sufficient space for the
  * buffer ref_array to hold the reference numbers of all lone vgroups,
  */
  ref_array = (int32 *) malloc(sizeof(int32) * nlones);
  
 /*
  * and call Vlone again to retrieve the reference numbers into 
  * the buffer ref_array.
  */
  nlones = Vlone (infile_id, ref_array, nlones);
  
 /*
  * iterate tru each lone vgroup.
  */
  for (i = 0; i < nlones; i++)
  {
  /*
   * attach to the current vgroup then get its
   * name and class. note: the current vgroup must be detached before
   * moving to the next.
   */
   vgroup_id = Vattach (infile_id, ref_array[i], "r");
   status_32 = Vgetname (vgroup_id, vgroup_name);
   status_32 = Vgetclass (vgroup_id, vgroup_class);
   
   /* ignore reserved HDF groups/vdatas */
   if( is_reserved(vgroup_class)){
    status_32 = Vdetach (vgroup_id);
    continue;
   }
   if(vgroup_name != NULL) 
    if(strcmp(vgroup_name,GR_NAME)==0) {
     status_32 = Vdetach (vgroup_id);
     continue;
    }
      
   /* 
    * create the group in the output file.  the vgroup reference number is set
    * to -1 for creating and the access mode is "w" for writing 
    */
    vgroup_id_out = Vattach (outfile_id, -1, "w");
    status_32     = Vsetname (vgroup_id_out, vgroup_name);
    status_32     = Vsetclass (vgroup_id_out, vgroup_class);

    copy_vgroup_attrs(vgroup_id, vgroup_id_out);

    if (options->verbose)
    printf(PFORMAT,"","",vgroup_name);    
       
    /* insert objects for this group */
    ntagrefs = Vntagrefs(vgroup_id);
    if ( ntagrefs > 0 )
    {
     tags = (int32 *) malloc(sizeof(int32) * ntagrefs);
     refs = (int32 *) malloc(sizeof(int32) * ntagrefs);
     Vgettagrefs(vgroup_id, tags, refs, ntagrefs);
     
     vgroup_insert(infname,outfname,infile_id,outfile_id,vgroup_id_out,vgroup_name,
                   tags,refs,ntagrefs,table,options);
     
     free (tags);
     free (refs);
    }
    
    status_32 = Vdetach (vgroup_id);
    status_32 = Vdetach (vgroup_id_out);

  } /* for */
  
  
  /* free the space allocated */
  if (ref_array) 
   free (ref_array);
 } /* if */
 

 /* terminate access to the V interface */
 status_n = Vend (infile_id);
 status_n = Vend (outfile_id);


}

/*-------------------------------------------------------------------------
 * Function: vgroup_insert
 *
 * Purpose: recursive function to locate objects in lone Vgroups
 *
 * Return: void
 *
 *-------------------------------------------------------------------------
 */

void vgroup_insert(char* infname,char* outfname,int32 infile_id,int32 outfile_id,
                   int32 vgroup_id_out_par, /* output parent group ID */
                   char*path_name,          /* absolute path for input group name */          
                   int32* in_tags,          /* tag list for parent group */
                   int32* in_refs,          /* ref list for parent group */
                   int npairs,              /* number tag/ref pairs for parent group */
                   table_t *table,
                   options_t *options)
{
 intn  status_n;              /* returned status_n for functions returning an intn  */
 int32 status_32,             /* returned status_n for functions returning an int32 */
       vgroup_id,             /* vgroup identifier */
       ntagrefs,              /* number of tag/ref pairs in a vgroup */
       sd_id,                 /* SD interface identifier */
       sd_out,                /* SD interface identifier */
       tag,                   /* temporary tag */
       ref,                   /* temporary ref */
       *tags,                 /* buffer to hold the tag numbers of vgroups   */
       *refs,                 /* buffer to hold the ref numbers of vgroups   */
       gr_id,                 /* GR interface identifier */
       gr_out,                /* GR interface identifier */
       vgroup_id_out,         /* vgroup identifier */
       vg_index;              /* position of a vgroup in the vgroup  */
 char  vgroup_name[VGNAMELENMAX], vgroup_class[VGNAMELENMAX];
 char  *path=NULL;
 int   i;
 
 for ( i = 0; i < npairs; i++ ) 
 {
  tag = in_tags[i];
  ref = in_refs[i];
  
  switch(tag) 
  {
/*-------------------------------------------------------------------------
 * VG
 *-------------------------------------------------------------------------
 */
  case DFTAG_VG: 
   
   vgroup_id = Vattach (infile_id, ref, "r");
   status_32 = Vgetname (vgroup_id, vgroup_name);
   status_32 = Vgetclass (vgroup_id, vgroup_class);
   
   /* ignore reserved HDF groups/vdatas */
   if( is_reserved(vgroup_class)){
    status_32 = Vdetach (vgroup_id);
    break;
   }
   if(vgroup_name != NULL) 
    if(strcmp(vgroup_name,GR_NAME)==0) {
     status_32 = Vdetach (vgroup_id);
     break;
    }

   /* initialize path */
   path=get_path(path_name,vgroup_name);

   /* add object to table; print always */
   table_add(table,tag,ref,path,options->verbose);

   if (options->verbose)
    printf(PFORMAT,"","",path);    
     
#if defined(HZIP_DEBUG)
   printf ("\t%s %d\n", path, ref); 
#endif
   
   if ( options->trip==0 ) 
   {
    /*we must go to other groups always */
   }
   
  /* 
   * create the group in the output file.  the vgroup reference number is set
   * to -1 for creating and the access mode is "w" for writing 
   */
   vgroup_id_out = Vattach (outfile_id, -1, "w");
   status_32     = Vsetname (vgroup_id_out, vgroup_name);
   status_32     = Vsetclass (vgroup_id_out, vgroup_class);

   copy_vgroup_attrs(vgroup_id, vgroup_id_out);
   
   /* insert the created vgroup into its parent */
   vg_index = Vinsert (vgroup_id_out_par, vgroup_id_out);
    
   /* insert objects for this group */
   ntagrefs  = Vntagrefs(vgroup_id);
   if ( ntagrefs > 0 )
   {
    tags = (int32 *) malloc(sizeof(int32) * ntagrefs);
    refs = (int32 *) malloc(sizeof(int32) * ntagrefs);
    Vgettagrefs(vgroup_id, tags, refs, ntagrefs);
    /* recurse */
    vgroup_insert(infname,outfname,infile_id,outfile_id,vgroup_id_out,
                  path,tags,refs,ntagrefs,table,options);
    free (tags);
    free (refs);
   }
   status_32 = Vdetach (vgroup_id);
   status_32 = Vdetach (vgroup_id_out);
   
   if (path)
    free(path);

   break;
   

/*-------------------------------------------------------------------------
 * SDS
 *-------------------------------------------------------------------------
 */   
   
  case DFTAG_SD:  /* Scientific Data */
  case DFTAG_SDG: /* Scientific Data Group */
  case DFTAG_NDG: /* Numeric Data Group */

   sd_id  = SDstart(infname, DFACC_RDONLY);
   sd_out = SDstart(outfname, DFACC_WRITE);

   /* copy dataset */
   copy_sds(sd_id,sd_out,tag,ref,vgroup_id_out_par,path_name,options,table);
 
   status_n = SDend (sd_id);
   status_n = SDend (sd_out);

   
   break;
   
/*-------------------------------------------------------------------------
 * Image
 *-------------------------------------------------------------------------
 */   
   
  case DFTAG_RI:  /* Raster Image */
  case DFTAG_CI:  /* Compressed Image */
  case DFTAG_RIG: /* Raster Image Group */

  case DFTAG_RI8:  /* Raster-8 image */
  case DFTAG_CI8:  /* RLE compressed 8-bit image */
  case DFTAG_II8:  /* IMCOMP compressed 8-bit image */

   gr_id  = GRstart(infile_id);
   gr_out = GRstart(outfile_id);

   /* copy GR  */
   copy_gr(gr_id,gr_out,tag,ref,vgroup_id_out_par,path_name,options,table);

   status_n = GRend (gr_id);
   status_n = GRend (gr_out);
   
   break;

/*-------------------------------------------------------------------------
 * Vdata
 *-------------------------------------------------------------------------
 */   
   
  case DFTAG_VH:  /* Vdata Header */
  case DFTAG_VS:  /* Vdata Storage */

   /* copy VS */
   copy_vs(infile_id,outfile_id,tag,ref,vgroup_id_out_par,path_name,options,table);
  
   break;
   
   
  }
  
 }
 
}


/*-------------------------------------------------------------------------
 * Function: list_gr
 *
 * Purpose: get top level GR images
 *
 * Return: void
 *
 *-------------------------------------------------------------------------
 */

void list_gr(char* infname,char* outfname,int32 infile_id,int32 outfile_id,table_t *table,options_t *options)
{
 intn  status;            /* status for functions returning an intn */
 int32 gr_id,             /* GR interface identifier */
       gr_out,            /* GR interface identifier */
       ri_id,             /* raster image identifier */
       n_rimages,         /* number of raster images in the file */
       n_file_attrs,      /* number of file attributes */
       ri_index,          /* index of a image */
       gr_ref,            /* reference number of the GR image */
       dim_sizes[2],      /* dimensions of an image */
       n_comps,           /* number of components an image contains */
       interlace_mode,    /* interlace mode of an image */ 
       data_type,         /* number type of an image */
       n_attrs;           /* number of attributes belong to an image */
 char  name[MAX_GR_NAME]; /* name of an image */
 
 /* initialize the GR interface */
 gr_id  = GRstart (infile_id);
 gr_out = GRstart (outfile_id);
 
 /* determine the contents of the file */
 status = GRfileinfo (gr_id, &n_rimages, &n_file_attrs);
  
 for (ri_index = 0; ri_index < n_rimages; ri_index++)
 {
  ri_id = GRselect (gr_id, ri_index);
  status = GRgetiminfo (ri_id, name, &n_comps, &data_type, &interlace_mode, 
   dim_sizes, &n_attrs);

  gr_ref = GRidtoref(ri_id);

  /* check if already inserted in Vgroup; search all image tags */
  if ( table_search(table,DFTAG_RI,gr_ref)>=0 ||
       table_search(table,DFTAG_CI,gr_ref)>=0 ||
       table_search(table,DFTAG_RIG,gr_ref)>=0 ||
       table_search(table,DFTAG_RI8,gr_ref)>=0 ||
       table_search(table,DFTAG_CI8,gr_ref)>=0 ||
       table_search(table,DFTAG_II8,gr_ref)>=0 )
  {
   status = GRendaccess (ri_id);
   continue;
  }

  /* copy GR  */
  copy_gr(gr_id,gr_out,DFTAG_RI,gr_ref,0,NULL,options,table);

  /* terminate access to the current raster image */
  status = GRendaccess (ri_id);
 }
 
 /* terminate access to the GR interface */
 status = GRend (gr_id);
 status = GRend (gr_out);
 

}


/*-------------------------------------------------------------------------
 * Function: list_sds
 *
 * Purpose: get top level SDS
 *
 * Return: void
 *
 *-------------------------------------------------------------------------
 */

void list_sds(char* infname,char* outfname,int32 infile_id,table_t *table,options_t *options)
{
 intn  status;                 /* status for functions returning an intn */
 int32 sd_id,                  /* SD interface identifier */
       sd_out,                 /* SD interface identifier */
       sds_id,                 /* dataset identifier */
       n_datasets,             /* number of datasets in the file */
       n_file_attrs,           /* number of file attributes */
       index,                  /* index of a dataset */
       sds_ref,                /* reference number */
       dim_sizes[MAX_VAR_DIMS],/* dimensions of an image */
       data_type,              /* number type  */
       rank,                   /* rank */
       n_attrs;                /* number of attributes */
 char  name[MAX_GR_NAME];      /* name of dataset */
 
 /* initialize the SD interface */
 sd_id  = SDstart (infname, DFACC_READ);
 sd_out = SDstart (outfname, DFACC_WRITE);
 
 /* determine the number of data sets in the file and the number of file attributes */
 status = SDfileinfo (sd_id, &n_datasets, &n_file_attrs);

 for (index = 0; index < n_datasets; index++)
 {
  sds_id  = SDselect (sd_id, index);
  status  = SDgetinfo(sds_id, name, &rank, dim_sizes, &data_type, &n_attrs);
  sds_ref = SDidtoref(sds_id);

  /* check if already inserted in Vgroup; search all SDS tags */
  if ( table_search(table,DFTAG_SD,sds_ref)>=0 ||
       table_search(table,DFTAG_SDG,sds_ref)>=0 ||
       table_search(table,DFTAG_NDG,sds_ref)>=0 )
  {
   status = SDendaccess (sds_id);
   continue;
  }

  /* copy SDS  */
  copy_sds(sd_id,sd_out,TAG_GRP_DSET,sds_ref,0,NULL,options,table);
     
  /* terminate access to the current dataset */
  status = SDendaccess (sds_id);
 }
 
 /* terminate access to the SD interface */
 status = SDend (sd_id);
 status = SDend (sd_out);
 

}

/*-------------------------------------------------------------------------
 * Function: list_vs
 *
 * Purpose: get top level VS
 *
 * Return: void
 *
 *-------------------------------------------------------------------------
 */


void list_vs(char* infname,char* outfname,int32 infile_id,int32 outfile_id,table_t *table,options_t *options)
{
 intn  status_n;     /* returned status_n for functions returning an intn  */
 int32 vdata_ref=-1, /* reference number of the vdata */
       nlones = 0,   /* number of lone vdatas */
       *ref_array,   /* buffer to hold the ref numbers of lone vdatas   */
       ref;          /* temporary ref number  */
 int   i;

 /* initialize the VS interface */
 status_n = Vstart (infile_id);
 status_n = Vstart (outfile_id);

/*
 * get and print the names and class names of all the lone vdatas.
 * first, call Vlone with nlones set to 0 to get the number of
 * lone vdatas in the file, but not to get their reference numbers.
 */
 nlones = VSlone (infile_id, NULL, nlones );

 if (nlones > 0)
 {
 /*
  * use the nlones returned to allocate sufficient space for the
  * buffer ref_array to hold the reference numbers of all lone vgroups,
  */
  ref_array = (int32 *) malloc(sizeof(int32) * nlones);
  
 /*
  * and call VSlone again to retrieve the reference numbers into 
  * the buffer ref_array.
  */
  nlones = VSlone (infile_id, ref_array, nlones);

 /*
  * iterate tru each lone vdata.
  */
  for (i = 0; i < nlones; i++)
  {
  /*
   * attach to the current vdata then get its
   * name and class. note: the current vdata must be detached before
   * moving to the next.
   */
   ref = ref_array[i];

   /* check if already inserted in Vgroup; search all VS tags */
   if ( table_search(table,DFTAG_VH,ref)>=0 ||
        table_search(table,DFTAG_VS,ref)>=0 ) {
    continue;
   }

   /* copy VS; the if test checks for reserved vdatas, that are not added to table  */
   copy_vs(infile_id,outfile_id,DFTAG_VS,ref,0,NULL,options,table);
 
  } /* for */

  
  /* free the space allocated */
  if (ref_array) free (ref_array);
 } /* if */

 /* terminate access to the VS interface */
 status_n = Vend (infile_id);
 status_n = Vend (outfile_id);



}

/*-------------------------------------------------------------------------
 * Function: is_reserved
 *
 * Purpose: check for reserved Vgroup/Vdata class/names
 *
 * Return: 1 if reserved, 0 if not
 *
 *-------------------------------------------------------------------------
 */

int is_reserved(char*vgroup_class)
{
 int ret=0;
 
 /* ignore reserved HDF groups/vdatas */
 if(vgroup_class != NULL) {
  if( (strcmp(vgroup_class,_HDF_ATTRIBUTE)==0) ||
   (strcmp(vgroup_class,_HDF_VARIABLE) ==0) || 
   (strcmp(vgroup_class,_HDF_DIMENSION)==0) ||
   (strcmp(vgroup_class,_HDF_UDIMENSION)==0) ||
   (strcmp(vgroup_class,DIM_VALS)==0) ||
   (strcmp(vgroup_class,DIM_VALS01)==0) ||
   (strcmp(vgroup_class,_HDF_CDF)==0) ||
   (strcmp(vgroup_class,GR_NAME)==0) ||
   (strcmp(vgroup_class,RI_NAME)==0) || 
   (strcmp(vgroup_class,RIGATTRNAME)==0) ||
   (strcmp(vgroup_class,RIGATTRCLASS)==0) ){
   ret=1;
  }

  /* class and name(partial) for chunk table i.e. Vdata */
  if( (strncmp(vgroup_class,"_HDF_CHK_TBL_",13)==0)){
   ret=1;
  }

 }
 
 return ret;
}

/*-------------------------------------------------------------------------
 * Function: copy_vgroup_attrs
 *
 * Purpose: copy VG attributes
 *
 * Return: 1 
 *
 *-------------------------------------------------------------------------
 */


int copy_vgroup_attrs(int32 vg_in, int32 vg_out) 
{
 int    n_attrs;
 int32  data_type, size,  n_values;
 char   attr_name[MAX_NC_NAME];
 int    i;
 char   *buf;

 /* Get the number of attributes attached to this vgroup.  */
 n_attrs = Vnattrs (vg_in);
 
 for (i = 0; i < n_attrs; i++) 
 {
  Vattrinfo (vg_in, i, attr_name, &data_type, &n_values, &size);
  buf = (char *)malloc(size * n_values);
  Vgetattr (vg_in, i, buf);
  Vsetattr(vg_out, attr_name, data_type, n_values, buf);
  free(buf);
 }
 return 1;
}


