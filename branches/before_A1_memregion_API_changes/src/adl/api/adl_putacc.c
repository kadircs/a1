/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *  Copyright (C) 2010 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "a1.h"
#include "a1d.h"
#include "a1u.h"

int A1_PutAcc(int target,
              void* source_ptr,
              void* target_ptr,
              int bytes,
              A1_datatype_t a1_type,
              void* scaling)
{
    int status = A1_SUCCESS;

    A1U_FUNC_ENTER();

    /* FIXME: The profiling interface needs to go here */

    /* FIXME: Locking functionality needs to go here */

#   ifdef HAVE_ERROR_CHECKING
#   endif

    status = A1D_PutAcc(target, source_ptr, target_ptr, bytes,
            a1_type, scaling);
    A1U_ERR_POP((status!=A1_SUCCESS), "A1D_PutAcc returned error\n");

  fn_exit:
    A1U_FUNC_EXIT();
    return status;

  fn_fail:
    goto fn_exit;
}

int A1_NbPutAcc(int target,
              void* source_ptr,
              void* target_ptr,
              int bytes,
              A1_datatype_t a1_type,
              void* scaling,
              A1_handle_t a1_handle)
{
    int status = A1_SUCCESS;

    A1U_FUNC_ENTER();

    /* FIXME: The profiling interface needs to go here */

    /* FIXME: Locking functionality needs to go here */

#   ifdef HAVE_ERROR_CHECKING
#   endif

    status = A1D_NbPutAcc(target, source_ptr, target_ptr, bytes,
            a1_type, scaling, a1_handle);
    A1U_ERR_POP((status!=A1_SUCCESS), "A1D_NbPutAcc returned error\n");

  fn_exit:
    A1U_FUNC_EXIT();
    return status;

  fn_fail:
    goto fn_exit;
}
