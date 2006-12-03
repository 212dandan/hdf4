/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group (THG).                                         *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF.  The full HDF copyright notice, including       *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file and in the print documentation copyright notice.         *
 * COPYING can be found at the root of the source code distribution tree;    *
 * the copyright notice in printed HDF documentation can be found on the     *
 * back of the title page.  If you do not have access to either version,     *
 * you may request a copy from help@hdfgroup.org.                            *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/****************************************************************************
 * tsdsprops.c - tests functionality regarding an SDS' properties, such
 *		 name, dimensions, type, size,...
 * Structure of the file:
 *    test_SDSprops - test driver
 *	  test_SDSnames    - tests that an SDS name can be more than 
 *		previously hardcoded at 64 characters; now length is variable.
 *	  test_unlim_dim - tests that writing two unlimited 1D datasets,
 *		without closing the file between writes, will not cause the
 *		second dataset to contain garbage anymore. (bug 525)
 *	  test_unlim_inloop - tests that appending unlimited data to more 
 *		than one dataset within a loop stores data correctly. (bug 801)
****************************************************************************/

#include "mfhdf.h"

#ifdef HDF

#include "hdftest.h"

/***************************************************************************
   Name: test_SDSnames() - tests that SDS name is no longer restricted to 
			   64 characters only. (bugzilla 516)
   Description:
	The main contents include:
	- create data sets with very short and very long names
	- close the file, then reopen it, get SDS' info, and verify the names

   Return value:
        The number of errors occurred in this routine.

   BMR - Oct 2006
****************************************************************************/

#define FILE_NAME     "SDSlongname.hdf"	/* data file to test SDS names */
#define X_LENGTH      10
#define Y_LENGTH      10
#define RANK          2
#define NAME_LEN1     93	/* the length of longname_ds below */
#define NAME_LEN2     1		/* the length of shortname_ds below */

#define	longname_ds	"The name of this dataset is long, and it is used to test the new variable length name feature"
#define	shortname_ds	"A"

static intn
test_SDSnames()
{
    int32  fid, dset1, dset2;
    int32  rank, dtype, nattrs, dimsizes[RANK];
    char*  ds_name;
    uint16 name_len;
    intn   status;
    intn   num_errs = 0;         /* number of errors so far */

    /* Create a file */
    fid = SDstart(FILE_NAME, DFACC_CREATE);
    CHECK(fid, FAIL, "SDstart");

    /* Create an X_LENGTH by Y_LENGTH dataset, called DataSet_1 */
    dimsizes[0] = X_LENGTH;
    dimsizes[1] = Y_LENGTH;
    dset1 = SDcreate(fid, longname_ds, DFNT_INT32, RANK, dimsizes);
    CHECK(dset1, FAIL, "SDcreate");

    dset2 = SDcreate(fid, shortname_ds, DFNT_INT32, RANK, dimsizes);
    CHECK(dset2, FAIL, "SDcreate");

    /* Close the datasets */
    status = SDendaccess(dset1);
    CHECK(status, FAIL, "SDendaccess");

    status = SDendaccess(dset2);
    CHECK(status, FAIL, "SDendaccess");

    /* Close the file */
    status = SDend(fid);
    CHECK(status, FAIL, "SDend");

    /* Re-open the file to check on the SDS name */
    fid = SDstart(FILE_NAME, DFACC_RDWR);
    CHECK(fid, FAIL, "SDstart");

    /* Get access to the first dataset */
    dset1 = SDselect(fid, 0);
    CHECK(dset1, FAIL, "SDselect");

    /* Get and verify the length of its name */
    status = SDgetnamelen(dset1, &name_len);
    CHECK(status, FAIL, "SDgetnamelen");
    VERIFY(name_len, NAME_LEN1, "SDgetnamelen");

    /* Allocate buffer to get its name */
    ds_name = (char *)HDmalloc(name_len+1);
    CHECK(ds_name, NULL, "HDmalloc");

    /* Get information of the first dataset, and verify its name */
    status = SDgetinfo(dset1, ds_name, &rank, dimsizes, &dtype, &nattrs);
    CHECK(status, FAIL, "SDgetinfo");
    VERIFY_CHAR(ds_name, longname_ds, "SDgetinfo");

    /* Close the datasets */
    status = SDendaccess(dset1);
    CHECK(status, FAIL, "SDendaccess");

    /* Get access to the second dataset */
    dset2 = SDselect(fid, 1);
    CHECK(dset2, FAIL, "SDselect");

    /* Get and verify the length of its name */
    name_len = 0;
    status = SDgetnamelen(dset2, &name_len);
    CHECK(status, FAIL, "SDgetnamelen");
    VERIFY(name_len, NAME_LEN2, "SDgetnamelen");

    /* Allocate buffer to get its name */
    ds_name = (char *)HDmalloc(name_len+1);
    CHECK(ds_name, NULL, "HDmalloc");

    /* Get information of the second dataset, and verify its name */
    status = SDgetinfo(dset2, ds_name, &rank, dimsizes, &dtype, &nattrs);
    CHECK(status, FAIL, "SDgetinfo");
    VERIFY_CHAR(ds_name, shortname_ds, "SDgetinfo");

    /* Close the datasets */
    status = SDendaccess(dset2);
    CHECK(status, FAIL, "SDendaccess");

    /* Close the file */
    status = SDend(fid);
    CHECK(status, FAIL, "SDend");

    /* Return the number of errors that's been kept track of, so far */
    return num_errs;
}   /* test_SDSnames */

/***************************************************************************
   Name: test_unlim_dim() - tests that writing two unlimited 1D datasets,
			    without closing the file between writes, will 
			    not cause the second dataset to contain garbage 
			    anymore. (bug 525)
   Description:
	The main contents include:
	- create and write to two datasets
	- append data to both without closing anything
	- close the datasets and the file
	- reopen the file, get the data sets and their info
	- read and verify the data

   Return value:
        The number of errors occurred in this routine.

   BMR - Oct 2005
****************************************************************************/

#define UD_FILE_NAME  "Unlim_dim.hdf"	/* data file to test unlimited dim */
#define DS1_NAME "data 1"
#define DS2_NAME "data 2"
#define DIM0 10

static intn
test_unlim_dim()
{
    int32 fid, dset1, dset2;
    int32 rank, start[1], edges[1], dtype, nattrs, dimsizes[1];
    int16 array_data[DIM0],	/* data to be written to both datasets */
	  append_data[DIM0],	/* data to be appended to both datasets */
	  outdata[DIM0+DIM0], outdata1[DIM0+DIM0]; /* data read */
    char  ds_name[20];
    intn  idx, status;
    intn  num_errs = 0;         /* number of errors so far */

    /* Create a file */
    fid = SDstart(UD_FILE_NAME, DFACC_CREATE);
    CHECK(fid, FAIL, "SDstart");

    /* Define the rank and dimensions of the data set to be created. */
    rank = 1;
    dimsizes[0] = SD_UNLIMITED;

    /* Create two array data sets. */
    dset1 = SDcreate(fid, DS1_NAME, DFNT_INT16, rank, dimsizes);
    CHECK(dset1, FAIL, "SDcreate");

    dset2 = SDcreate(fid, DS2_NAME, DFNT_INT16, rank, dimsizes);
    CHECK(dset2, FAIL, "SDcreate");

    /* Fill the stored-data array with values. */
    for (idx = 0; idx < DIM0; idx++) {
	array_data[idx] = idx + 1;
    }

    /* Define the location, pattern, and size of the data sets */
    start[0] = 0;
    edges[0] = DIM0;

    status = SDwritedata(dset1, start, NULL, edges, (VOIDP)array_data);
    CHECK(status, FAIL, "SDwritedata");
    status = SDwritedata(dset2, start, NULL, edges, (VOIDP)array_data);
    CHECK(status, FAIL, "SDwritedata");

    /* Store array values to be appended */
    for (idx = 0; idx <DIM0; idx++)
	append_data[idx] = DIM0 + idx;

    /* Define the location of the append */
    start[0] = DIM0; /* start from the end of the previous write */
    edges[0] = DIM0; /* append the same number of elements as the write */

    /* Append data to file */
    status = SDwritedata (dset1, start, NULL, edges, (VOIDP) append_data);
    CHECK(status, FAIL, "SDwritedata");
    status = SDwritedata (dset2, start, NULL, edges, (VOIDP) append_data);
    CHECK(status, FAIL, "SDwritedata");

    /* Close the datasets */
    status = SDendaccess(dset1);
    CHECK(status, FAIL, "SDendaccess");

    status = SDendaccess(dset2);
    CHECK(status, FAIL, "SDendaccess");

    /* Close the file */
    status = SDend(fid);
    CHECK(status, FAIL, "SDend");

    /* Re-open the file to check on the SDS names */
    fid = SDstart(UD_FILE_NAME, DFACC_RDWR);
    CHECK(fid, FAIL, "SDstart");

    /* Get access to the datasets */
    dset1 = SDselect(fid, 0);
    CHECK(dset1, FAIL, "SDselect");
    dset2 = SDselect(fid, 1);
    CHECK(dset1, FAIL, "SDselect");

    /* Get information of the first dataset, and verify its name */
    status = SDgetinfo(dset1, ds_name, &rank, dimsizes, &dtype, &nattrs);
    CHECK(status, FAIL, "SDgetinfo");

    /* Define the location to read */
    start[0] = 0;
    edges[0] = DIM0+DIM0;

    /* Read and check first dataset */
    status = SDreaddata(dset1, start, NULL, edges, (VOIDP) outdata);
    CHECK(status, FAIL, "SDreaddata");

    for (idx = 0; idx < DIM0; idx++) {
	VERIFY(outdata[idx], array_data[idx], "SDreaddata first dataset");
    }
    for (idx = DIM0; idx < edges[0]; idx++) {
	VERIFY(outdata[idx], append_data[idx-DIM0], "SDreaddata first dataset");
    }

    /* Read and check second dataset */
    status = SDreaddata(dset2, start, NULL, edges, (VOIDP) outdata1);
    CHECK(status, FAIL, "SDreaddata");

    for (idx = 0; idx < DIM0; idx++) {
	VERIFY(outdata1[idx], array_data[idx], "SDreaddata first dataset");
    }
    for (idx = DIM0; idx < edges[0]; idx++) {
	VERIFY(outdata1[idx], append_data[idx-DIM0], "SDreaddata first dataset");
    }

    /* Close the datasets */
    status = SDendaccess(dset1);
    CHECK(status, FAIL, "SDendaccess");

    /* Close the datasets */
    status = SDendaccess(dset2);
    CHECK(status, FAIL, "SDendaccess");

    /* Close the file */
    status = SDend(fid);
    CHECK(status, FAIL, "SDend");

    /* Return the number of errors that's been kept track of, so far */
    return num_errs;
}   /* test_unlim_dim */

/***************************************************************************
   Name: test_unlim_inloop() - tests that appending unlimited data to more
			    than one dataset within a loop stores data
			    correctly. (bug 801)
   Description:
	The main contents include:
	- create two data sets 
	- write to the data sets in a loop
	- close the file, then reopen it
	- in another loop, read the data sets and verify their data

   Return value:
        The number of errors occurred in this routine.

   BMR - Oct 2005
****************************************************************************/

#define UDIL_FILE_NAME  "Unlim_inloop.hdf" /* file to test unlim dims in loop */
#define SIZE 5 
#define N_DSETS 2 
#define RANK1 1 

static intn
test_unlim_inloop()
{
    int32   fid;
    int32   sds_id[N_DSETS];
    int32   start[RANK1], edges[RANK1], rank, dimsizes[RANK1];
    int32   dtype, nattrs;
    char    sds_name[20];
    int     i, n_writes; /* number of times writing to the data sets */
    /*int32   n,m, mm;*/
    intn    status;
    float64 array_data[SIZE];
    intn    num_errs = 0;         /* number of errors so far */

    /* Create a file */
    fid = SDstart(UDIL_FILE_NAME, DFACC_CREATE);
    CHECK(fid, FAIL, "SDstart");

    dimsizes[0] = SD_UNLIMITED;

    /* Create N_DSETS data sets in a loop */
    for(i = 0; i < N_DSETS; i++) {
        sprintf(sds_name,"test%d",i);
        sds_id[i] = SDcreate (fid, sds_name, DFNT_FLOAT64, 1, dimsizes);
        CHECK(sds_id[i], FAIL, "SDcreate");

        status = SDendaccess (sds_id[i]);
        CHECK(status, FAIL, "SDendaccess");
     }

    /* Making up data to write */
    for (i = 0; i < SIZE; i++) {
	array_data[i] = i + 1;
    } 

    /* write and append to the data sets */
    n_writes = 0;
    edges[0] = SIZE;
    while (n_writes < 2)
    {
	/* i.e, start at 0 first time and at SIZE second time*/
	start[0] = SIZE * n_writes;

	for (i = 0; i < N_DSETS; i++) 
	{
	    sds_id[i] = SDselect(fid, i);
	    CHECK(sds_id[i], FAIL, "SDselect");

	    status = SDwritedata(sds_id[i], start, NULL, edges, (VOIDP)array_data);
	    CHECK(status, FAIL, "SDwritedata");

	    status = SDendaccess (sds_id[i]);
	    CHECK(status, FAIL, "SDendaccess");
	}
	n_writes++;
    }

    /* close the file */
    status = SDend (fid);
    CHECK(status, FAIL, "SDcreate");

    /* Re-open the file to check on the SDS name */
    fid = SDstart(UDIL_FILE_NAME, DFACC_RDWR);
    CHECK(fid, FAIL, "SDstart");

    for (i = 0; i < N_DSETS; i++)
    {
	float64 outdata[SIZE*2];

	/* Get access to the datasets */
	sds_id[i] = SDselect(fid, i);

	/* Get information of the first dataset, and verify its name */
	status = SDgetinfo(sds_id[i], sds_name, &rank, dimsizes, &dtype, &nattrs);
	CHECK(status, FAIL, "SDgetinfo");

	/* Define the location to read */
	start[0] = 0;
	edges[0] = SIZE * n_writes;

	/* Read and check first dataset */
	status = SDreaddata(sds_id[i], start, NULL, edges, (VOIDP) outdata);
	CHECK(status, FAIL, "SDreaddata");

	/* Verify the read data */
	n_writes = 0;
	while (n_writes < 2)
	{
	    intn in, out;
	    for (in = 0, out = 0+(SIZE*n_writes); in < SIZE; in++, out++) {
                VERIFY(outdata[out], array_data[in], "SDreaddata");
	    }
	    n_writes++;
	}

	/* Close the datasets */
	status = SDendaccess(sds_id[i]);
	CHECK(status, FAIL, "SDendaccess");
    }

    /* close the file */
    status = SDend (fid);
    CHECK(status, FAIL, "SDcreate");

    /* Return the number of errors that's been kept track of, so far */
    return num_errs;
}   /* test_unlim_inloop */

/* Test driver for testing various SDS' properties. */
extern int
test_SDSprops()
{
    intn num_errs = 0;         /* number of errors */

    num_errs = num_errs + test_SDSnames();
    num_errs = num_errs + test_unlim_dim();
    num_errs = num_errs + test_unlim_inloop();

    return num_errs;
}

#endif /* HDF */
