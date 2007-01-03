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

#include "mfhdf.h"

#ifdef HDF

#include "hdftest.h"

/********************************************************************
   Name: test_file_inuse() - tests preventing of an in-use file being 
			     removed at cleanup time.

   Description: 
	Sometime, when an error occurs, the cleanup process attempts to 
	remove a file, which might still be in use (part of bugzilla #376.)  
	The routine test_file_inuse is to test the fix that provides the 
	underlaying call to HPisfile_in_use, which should successfully 
	determines whether a file is still in use before an attempt to remove.

	The main contents include:
	- a loop that repeatedly calls SDstart/DFACC_CREATE; only the first
	  SDstart succeeds, the subsequent ones should fail.
	- SDcreate, SDwritedata, SDendaccess follow
	- outside of that loop is another loop to call SDend corresponding
	  to the previous SDstart's
	- then, at the end, the file will be reopened; if the file doesn't
	  exist and causes SDstart to fail, the test will fail.

	Before the fix, when the 2nd SDstart/DFACC_CREATE was called and
	failed because the file was being in use from the first call to
	SDstart/DFACC_CREATE, the cleaning process removed the file.

   Return value:
	The number of errors occurred in this routine.

   BMR - Jun 22, 2005
*********************************************************************/

#define FILE_NAME     "bug376.hdf"	/* data file to test */
#define DIM0 10

static intn
test_file_inuse()
{
    int32 file_id, sd_id[5], sds_id[5];
    intn statusn;
    int32 dims[1], start[1], edges[1], rank;
    int16 array_data[DIM0];
    char* names[5] = {"data1", "data2", "data3", "data4", "data5"};
    intn i, j;
    intn      num_errs = 0;     /* number of errors so far */

    for (i=0; i<5; i++)
    {
        /* Create and open the file and initiate the SD interface. */
        sd_id[i] = SDstart(FILE_NAME, DFACC_CREATE);
	if (i == 0) {
	    CHECK(sd_id[i], FAIL, "SDstart"); } /* 1st SDstart must pass */
	else {
	    VERIFY(sd_id[i], FAIL, "SDstart"); }
	/* subsequent SDstart should fail, which causes the following calls
	   to fail as well */

        /* Define the rank and dimensions of the data sets to be created. */
        rank = 1;
        dims[0] = DIM0;
        start[0] = 0;
        edges[0] = DIM0;

        /* Create the array data set. */
        sds_id[i] = SDcreate(sd_id[i], names[i], DFNT_INT16, rank, dims);
	if (i == 0) {
	    CHECK(sds_id[i], FAIL, "SDcreate"); } /* 1st SDcreate must pass */
	else
	    VERIFY(sds_id[i], FAIL, "SDcreate");

        /* Fill the stored-data array with values. */
        for (j = 0; j < DIM0; j++) {
                        array_data[j] = (i + 1)*(j + 1);
        }

	/* Write data to the data set */
	statusn = SDwritedata(sds_id[i], start, NULL, edges, (VOIDP)array_data);
	if (i == 0) {
	    CHECK(statusn, FAIL, "SDwritedata"); } /* 1st SDwritedata must pass */
	else
	    VERIFY(statusn, FAIL, "SDwritedata");

        /* Terminate access to the data sets. */
        statusn = SDendaccess(sds_id[i]);
	if (i == 0) {
	    CHECK(statusn, FAIL, "SDendaccess"); } /* 1st SDendaccess must pass */
	else
	    VERIFY(statusn, FAIL, "SDendaccess");

    } /* for i */

    for (i=0; i<5; i++)
    {
        /* Terminate access to the SD interface and close the file. */
        statusn = SDend (sd_id[i]);
	if (i == 0) {
	    CHECK(statusn, FAIL, "SDend"); } /* 1st SDend must pass */
	else
	    VERIFY(statusn, FAIL, "SDend");
    }

    /* Try to open the file, which should exist */
    file_id = SDstart(FILE_NAME, DFACC_RDWR);
    CHECK(file_id, FAIL, "SDstart");

    statusn = SDend (file_id);
    CHECK(statusn, FAIL, "SDend");

    return num_errs;
}   /* test_file_inuse */

/********************************************************************
   Name: test_max_open_files() - tests the new API SDreset_maxopenfiles,
				SDget_maxopenfiles, SDget_numopenfiles,
				and SDgetfilename.

   Description: 
	There were multiple requests from the users to increase the maximum
	number of opened files allowed.  SDreset_maxopenfiles is added to
	allow the user to reset that value.  The current default value is 32.
	This API can be called anytime to increase it.  This test routine will 
	carry out the following tests:

	- Get the current max, should be the default (32,) and the system limit
	- Reset current max to an arbitrary number that is larger than the 
	  default and verify
	- Try to create more files than the current max and all should 
	  succeed, because NC_open resets the current max to system limit 
	  automatically, when the number of opened files exceeds the current
	  max
	- Get the current max and system limit and verify, current max 
	  should be the system limit
	- Get the current max another way, it should be the system limit again
	- Get the current number of files being opened
	- Reset current max to a value that is smaller than the current
	  number of opened files; it shouldn't reset
	- Reset current max again to a value that is smaller than the
	  current max but larger than the current number of opened files,
	  that should work for there is no information loss
	- Try to create more files up to the system limit or NUM_FILES_HI,
	  because the arrays have max NUM_FILES_HI elements in this test
	- Close all the files, then try opening all again to verify their 
	  names, this is to test bugzilla 440

   Return value:
	The number of errors occurred in this routine.

   BMR - Oct 14, 2005
*********************************************************************/

#define NUM_FILES_LOW 35
#define NUM_FILES_HI  1024

static int
test_max_open_files()
{
    int32 fids[NUM_FILES_HI];		/* holds IDs of opened files */
    char  filename[NUM_FILES_HI][10];	/* holds generated file names */
    char  readfname[MAX_NC_NAME];	/* file name retrieved from file id */
    intn  index, status,
	  curr_max, 	/* curr maximum number of open files allowed in HDF */
	  sys_limit, 	/* maximum number of open files allowed by system */
	  curr_max_bk,  /* back up of curr_max */
	  curr_opened,	/* number of files currently being opened */
	  temp_limit,	/* temp var - num of files to be opened in this test */
	  num_errs = 0;	/* number of errors so far */

    /* Get the current max and system limit */
    status = SDget_maxopenfiles(&curr_max, &sys_limit);
    VERIFY(curr_max, MAX_NC_OPEN, "test_maxopenfiles: SDreset_maxopenfiles");

    /* Reset current max to an arbitrary number and check */
    curr_max = SDreset_maxopenfiles(33);
    VERIFY(curr_max, 33, "test_maxopenfiles: SDreset_maxopenfiles");

    /* Try to create more files than the default max (currently, 32) and
       all should succeed */
    for (index=0; index < NUM_FILES_LOW; index++)
    {
    /* Create a file */
	sprintf(filename[index], "file%i", index);
	fids[index] = SDstart(filename[index], DFACC_CREATE);
	CHECK(fids[index], FAIL, "test_maxopenfiles: SDstart");
    }

    /* Get the current max and system limit */
    status = SDget_maxopenfiles(&curr_max, &sys_limit);
    VERIFY(curr_max, sys_limit, "test_maxopenfiles: SDreset_maxopenfiles");

    /* Get the current max another way, it should be the system limit */
    curr_max = SDreset_maxopenfiles(0);
    VERIFY(curr_max, sys_limit, "test_maxopenfiles: SDreset_maxopenfiles");

    /* Get the number of files currently being opened */
    curr_opened = SDget_numopenfiles();

    /* Reset current max to a value that is smaller than the current 
       number of opened files; it shouldn't reset */
    curr_max_bk = curr_max;
    curr_max = SDreset_maxopenfiles(curr_opened-1);
    VERIFY(curr_max, curr_max_bk, "test_maxopenfiles: SDreset_maxopenfiles");

    /* Reset current max again to a value that is smaller than the 
       current max but larger than the current number of opened files, 
       that should work for there is no information loss */
    curr_max = SDreset_maxopenfiles(curr_opened+3);
    VERIFY(curr_max, curr_opened+3, "test_maxopenfiles: SDreset_maxopenfiles");

    /* Try to create more files up to the system limit or NUM_FILES_HI,
       because the arrays have max NUM_FILES_HI elements in this test */
    temp_limit = sys_limit / 2;
    temp_limit = temp_limit > NUM_FILES_HI ? NUM_FILES_HI : temp_limit;
    for (index=NUM_FILES_LOW; index < temp_limit; index++)
    {
        /* Create a file */
	sprintf(filename[index], "file%i", index);
	fids[index] = SDstart(filename[index], DFACC_CREATE);

        /* if SDstart fails due to "too many open files," then adjust
           temp_limit so that further failure can be prevented, i.e.
           following SDend and SDstart */
        if ((fids[index] == FAIL) && (HEvalue(1) == DFE_TOOMANY))
            temp_limit = index;

        /* only CHECK returned value from SDstart if the failure wasn't
           because of "too many open files" -BMR 2006/11/01 */
        else
            CHECK(fids[index], FAIL, "test_maxopenfiles: SDstart");
    }

    /* Close all the files, then try opening all again to verify their 
       names, this is to test bugzilla 440 */
    for (index=0; index < temp_limit; index++)
    {
	status = SDend(fids[index]);
	CHECK(status, FAIL, "test_maxopenfiles: SDend");

	fids[index] = SDstart(filename[index], DFACC_RDWR);
	CHECK(fids[index], FAIL, "test_maxopenfiles: SDstart");
    }

    /* Verify their names */
    for (index=0; index < temp_limit; index++)
    {
	status = SDgetfilename(fids[index], readfname);
	CHECK(status, FAIL, "test_maxopenfiles: SDgetfilename");

	/* Verify the file name retrieved against the original */
	if (HDstrcmp(readfname, filename[index]))
	{
	    fprintf(stderr, "SDgetfilename: incorrect file being opened - expected <%s>, retrieved <%s>\n", filename[index], readfname);
	}
    }

    /* Close then remove all the files */
    for (index=0; index < temp_limit; index++)
    {
	status = SDend(fids[index]);
	CHECK(status, FAIL, "test_maxopenfiles: SDend");
	remove(filename[index]);
    }
    return num_errs;
}

/* Test driver for testing miscellaneous file related APIs. */
extern int
test_files()
{
    intn num_errs = 0;         /* number of errors */

    /* test that an in-use file is not removed in certain failure 
       cleanup. 06/21/05 - bugzilla 376 - BMR */
    num_errs = num_errs + test_file_inuse();

    /* test APIs that were added for fixing bugzilla 396 and 440.  
       09/07/05 - BMR */
    num_errs = num_errs + test_max_open_files();

    return num_errs;
}

#endif /* HDF */

