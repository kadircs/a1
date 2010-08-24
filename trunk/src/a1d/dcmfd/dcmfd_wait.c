/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *  Copyright (C) 2010 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "dcmfdimpl.h"

int A1D_Wait_handle(A1_handle_t a1_handle)
{
    int status = A1_SUCCESS;
    A1D_Handle_t *a1d_handle;

    A1U_FUNC_ENTER();

    A1DI_CRITICAL_ENTER();

    a1d_handle = (A1D_Handle_t *) a1_handle;

    A1DI_Conditional_advance(a1d_handle->active > 0);

  fn_exit:
    A1DI_CRITICAL_EXIT();
    A1U_FUNC_EXIT();
    return status;

  fn_fail:
    goto fn_exit;
}


int A1D_Wait_handle_list(int count, A1_handle_t *a1_handle)
{
    int status = A1_SUCCESS;
    int composite_active = A1_TRUE;
    A1D_Handle_t *a1d_handle;

    A1U_FUNC_ENTER();

    A1DI_CRITICAL_ENTER();

    /* TODO: verify that this is the correct implementation */
    do {
        A1DI_Advance();
        for(i=0;i<count;i++)
        {
            a1d_handle = (A1D_Handle_t *) a1_handle[i];
            composite_active = composite_active || ( (a1d_handle->active > 0) ? A1_FALSE : A1_TRUE );
        }
    } while(composite_active == A1_TRUE)

  fn_exit:
    A1DI_CRITICAL_EXIT();
    A1U_FUNC_EXIT();
    return status;

  fn_fail:
    goto fn_exit;
}

int A1D_Test_handle(A1_handle_t a1_handle, A1_bool_t* completed)
{
    int status = A1_SUCCESS;
    A1D_Handle_t *a1d_handle;

    A1U_FUNC_ENTER();

    A1DI_CRITICAL_ENTER();

    a1d_handle = (A1D_Handle_t *) a1_handle;
    A1DI_Advance();
    *completed = (a1d_handle->active > 0) ? A1_FALSE : A1_TRUE;

  fn_exit:
    A1DI_CRITICAL_EXIT();
    A1U_FUNC_EXIT();
    return status;

  fn_fail:
    goto fn_exit;
}


int A1D_Test_handle_list(int count, A1_handle_t *a1_handle, A1_bool_t* *completed)
{
    int i;
    int status = A1_SUCCESS;
    A1D_Handle_t *a1d_handle;

    A1U_FUNC_ENTER();

    A1DI_CRITICAL_ENTER();

    /* TODO: verify that this is the correct implementation */
    A1DI_Advance();
    for(i=0;i<count;i++)
    {
        a1d_handle = (A1D_Handle_t *) a1_handle[i];
        *completed[i] = (a1d_handle->active > 0) ? A1_FALSE : A1_TRUE;
    }

  fn_exit:
    A1DI_CRITICAL_EXIT();
    A1U_FUNC_EXIT();
    return status;

  fn_fail:
    goto fn_exit;
}
