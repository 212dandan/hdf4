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

#ifdef RCSID
static char RcsId[] = "@(#)$Revision$";
#endif

/* $Id$ */

/*-----------------------------------------------------------------------------
 * File:    dfjpeg.c
 * Purpose: JPEG image compression algorithm
 * Invokes: JPEG library functions
 * Contents:
 *  DFCIjpeg: compress image using JPEG compression
 * Remarks: DFCIjpeg() compress images using the JPEG library functions.
 *      This file (dfjpeg.c) and dfunjpeg.c should remain the only HDF files
 *      that has to know about how to use the JPEG routines.
 *---------------------------------------------------------------------------*/

#include "hdf.h"
#include "jpeglib.h"
#include "jerror.h"

/* Expanded data destination object for HDF output */

typedef struct {
    struct jpeg_destination_mgr pub; /* public fields */

    int32 aid;              /* target AID for output */
    int32 file_id;          /* HDF file ID */
    uint16 tag, ref;        /* tag & ref of image to output */
    VOIDP image;            /* pointer to the image data */
    int32 xdim, ydim;       /* X & Y dimensions of the image */
    int16 scheme;           /* type of image (8-bit or 24-bit) */

    JOCTET *buffer;         /* buffer for JPEG library to fill */
} hdf_destination_mgr;

typedef hdf_destination_mgr * hdf_dest_ptr;

#define OUTPUT_BUF_SIZE     4096    /* size of JPEG output buffer */

/*-----------------------------------------------------------------------------
 * Name:    hdf_init_destination
 * Purpose: Initialize the destination mgr for the JPEG image
 * Inputs:
 *      cinfo_ptr - JPEG compression structure pointer
 * Returns: none.
 * Users:   JPEG library
 * Invokes: HDF low-level I/O functions
 * Remarks: Initializes the JPEG destination mgr for further output.
 *---------------------------------------------------------------------------*/
void
hdf_init_destination(struct jpeg_compress_struct *cinfo_ptr)
{
    hdf_dest_ptr dest=(hdf_dest_ptr)cinfo_ptr->dest;
    int32 temp_aid;

    if((dest->buffer=HDmalloc(sizeof(JOCTET)*OUTPUT_BUF_SIZE))==NULL)
        ERREXIT1(cinfo_ptr, JERR_OUT_OF_MEMORY, (int)1);

    /* Create empty JPEG5/GREYJPEG5 tag/ref to indicate the image */
    if((temp_aid=Hstartwrite(dest->file_id,dest->scheme,dest->ref,0))==FAIL)
        ERREXIT(cinfo_ptr, JERR_FILE_WRITE);
    Hendaccess(temp_aid);

    if((dest->aid=Hstartaccess(dest->file_id,dest->tag,dest->ref,DFACC_WRITE|DFACC_APPENDABLE))==FAIL)
        ERREXIT(cinfo_ptr, JERR_FILE_WRITE);

    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
} /* end hdf_init_destination() */

/*-----------------------------------------------------------------------------
 * Name:    hdf_empty_output_buffer
 * Purpose: Write out a chunk of JPEG compressed data
 * Inputs:
 *      cinfo_ptr - JPEG compression structure pointer
 * Returns: none.
 * Users:   JPEG library
 * Invokes: HDF low-level I/O functions
 * Remarks: 
 *---------------------------------------------------------------------------*/
boolean
hdf_empty_output_buffer(struct jpeg_compress_struct *cinfo_ptr)
{
    hdf_dest_ptr dest=(hdf_dest_ptr)cinfo_ptr->dest;

    if(Hwrite(dest->aid,OUTPUT_BUF_SIZE,dest->buffer)!=OUTPUT_BUF_SIZE)
        ERREXIT(cinfo_ptr, JERR_FILE_WRITE);

    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
    return TRUE;
} /* end hdf_empty_output_buffer() */

/*-----------------------------------------------------------------------------
 * Name:    hdf_term_destination
 * Purpose: Terminate the destination mgr for the JPEG image
 * Inputs:
 *      cinfo_ptr - JPEG compression structure pointer
 * Returns: none.
 * Users:   JPEG library
 * Invokes: HDF low-level I/O functions
 * Remarks: Terminate the JPEG destination mgr for further output.
 *---------------------------------------------------------------------------*/
void
hdf_term_destination(struct jpeg_compress_struct *cinfo_ptr)
{
    hdf_dest_ptr dest=(hdf_dest_ptr)cinfo_ptr->dest;
    size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

    /* Write any data remaining in the buffer */
    if (datacount > 0) {
        if (Hwrite(dest->aid, datacount, dest->buffer) != datacount)
            ERREXIT(cinfo_ptr, JERR_FILE_WRITE);
    }

    /* close the HDF object */
    Hendaccess(dest->aid);

    /* Free the output buffer */
    HDfree(dest->buffer);

} /* end hdf_term_destination() */

/*-----------------------------------------------------------------------------
 * Name:    jpeg_HDF_dest
 * Purpose: Setup an HDF I/O destination manager for the IJG library.
 * Inputs:
 *      cinfo_ptr - JPEG compression structure pointer
 *      file_id - HDF file ID for file we are opening
 *      tag - HDF tag for image we are writing
 *      ref - HDF ref for image we are writing
 *      image - pointer to the image data to output
 *      xdim,ydim - X & Y dimensions of image
 *      scheme - the type of image to output
 * Returns: 0 on success, -1 on failure
 * Users:   DFCIjpeg
 * Invokes: JPEG library routines (lots of them...)
 * Remarks: Sets up the destination manager functions for the JPEG library.
 *          These routines will be called by the JPEG routines to output
 *---------------------------------------------------------------------------*/
intn
jpeg_HDF_dest(struct jpeg_compress_struct *cinfo_ptr, int32 file_id, uint16 tag,
    uint16 ref, VOIDP image, int32 xdim, int32 ydim, int16 scheme)
{
    CONSTR(FUNC, "jpeg_HDF_dest");     /* for HERROR */
    hdf_dest_ptr dest;

    if((dest=HDmalloc(sizeof(hdf_destination_mgr)))==NULL)
        HRETURN_ERROR(DFE_NOSPACE,FAIL);

    cinfo_ptr->dest=(struct jpeg_destination_mgr *)dest;
    dest->pub.init_destination = hdf_init_destination;
    dest->pub.empty_output_buffer = hdf_empty_output_buffer;
    dest->pub.term_destination = hdf_term_destination ;

    /* Now the HDF specific parameters */
    dest->aid = 0;  /* start with no AID */
    dest->file_id = file_id;
    dest->tag = tag;
    dest->ref = ref;
    dest->image = image;
    dest->xdim = xdim;
    dest->ydim = ydim;
    dest->scheme = scheme;

    return(SUCCEED);
} /* end jpeg_HDF_dest() */

/*-----------------------------------------------------------------------------
 * Name:    jpeg_HDF_dest_term
 * Purpose: Terminate an HDF I/O destination manager for the IJG library.
 * Inputs:
 *      cinfo_ptr - JPEG compression structure pointer
 * Returns: 0 on success, -1 on failure
 * Users:   DFCIjpeg
 * Invokes: JPEG library routines (lots of them...)
 * Remarks: Terminates the destination manager functions for the JPEG library.
 *---------------------------------------------------------------------------*/
intn
jpeg_HDF_dest_term(struct jpeg_compress_struct *cinfo_ptr)
{
    /* all we need to do for now is to free up the dest. mgr structure */
    HDfree(cinfo_ptr->dest);

    return(SUCCEED);
} /* end jpeg_HDF_dest_term() */

/***********************************************************************/
/* HDF callable routine for writing out an image with JPEG compression */
/***********************************************************************/

/*-----------------------------------------------------------------------------
 * Name:    DFCIjpeg
 * Purpose: compress an image using the JPEG compression algorithm
 * Inputs:
 * Returns: 0 on success, -1 on failure
 * Users:   HDF programmers, DFputcomp, other routines
 * Invokes: JPEG library routines (lots of them...)
 * Remarks: Uses the JPEG library routines.  The reason this routine doesn't
 *          compress into a buffer (like IMCOMP and RLE methods) is because
 *          the size of the buffer cannot be predicted before-hand and since
 *          24-bit images are already huge, I don't want to try allocating a
 *          worst-case buffer.  This means that this routine has to do the
 *          writing of the compressed image itself, instead of allowing
 *          DFputcomp() to write out the entire image at once.
 *---------------------------------------------------------------------------*/

intn
DFCIjpeg(int32 file_id, uint16 tag, uint16 ref, int32 xdim, int32 ydim,
         VOIDP image, int16 scheme, comp_info * scheme_info)
{
    CONSTR(FUNC, "DFCIjpeg");     /* for HERROR */
    /* These three structs contain JPEG parameters and working data.
     * They must survive for the duration of parameter setup and one
     * call to jpeg_compress; typically, making them local data in the
     * calling routine is the best strategy.
     */
    struct jpeg_compress_struct *cinfo_ptr;
    struct jpeg_error_mgr *jerr_ptr;
    JSAMPROW row_pointer[1];
    intn row_stride;
    uint8 *image_buffer=(uint8 *)image;

    if((cinfo_ptr=HDmalloc(sizeof(struct jpeg_compress_struct)))==NULL)
        HRETURN_ERROR(DFE_NOSPACE,FAIL);

    if((jerr_ptr=HDmalloc(sizeof(struct jpeg_error_mgr)))==NULL)
        HRETURN_ERROR(DFE_NOSPACE,FAIL);

    /* Initialize the error-handling routines */
    cinfo_ptr->err=jpeg_std_error(jerr_ptr);

    /* Initialize the JPEG compression stuff */
    jpeg_create_compress(cinfo_ptr);

    /* Set-up HDF destination manager */
    jpeg_HDF_dest(cinfo_ptr,file_id,tag,ref,image,xdim,ydim,scheme);

    /* Set up default JPEG parameters in the cinfo data structure. */
    cinfo_ptr->image_width=xdim;
    cinfo_ptr->image_height=ydim;
    if((uint16)scheme==DFTAG_JPEG5) /* 24-bit image */
      {
        cinfo_ptr->input_components=3;
        cinfo_ptr->in_color_space=JCS_RGB;
        row_stride=xdim*3;
      } /* end if */
    else if((uint16)scheme==DFTAG_GREYJPEG5) /* 8-bit image */
      {
        cinfo_ptr->input_components=1;
        cinfo_ptr->in_color_space=JCS_GRAYSCALE;
        row_stride=xdim;
      } /* end if */
    else
        HRETURN_ERROR(DFE_ARGS,FAIL);
    jpeg_set_defaults(cinfo_ptr);

    /* Set up user JPEG parameters in the cinfo data structure. */
    jpeg_set_quality(cinfo_ptr, scheme_info->jpeg.quality,
                 scheme_info->jpeg.force_baseline);

    /* OK, get things started */
    jpeg_start_compress(cinfo_ptr,TRUE);

    /* write the whole image out at once */
    while (cinfo_ptr->next_scanline < cinfo_ptr->image_height)
      {
        row_pointer[0]=&image_buffer[cinfo_ptr->next_scanline * row_stride];
        jpeg_write_scanlines(cinfo_ptr,row_pointer,1);
      } /* end while */

    /* Finish writing stuff out */
    jpeg_finish_compress(cinfo_ptr);

    /* Get rid of the JPEG information */
    jpeg_destroy_compress(cinfo_ptr);

    /* Wrap up any HDF specific code */
    jpeg_HDF_dest_term(cinfo_ptr);

    /* Free update memory allocated */
    HDfree(jerr_ptr);
    HDfree(cinfo_ptr);

    return (SUCCEED);   /* we must be ok... */
}   /* end DFCIjpeg() */

