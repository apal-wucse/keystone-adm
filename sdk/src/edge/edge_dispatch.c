//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "edge_call.h"
#include "edge_common.h"

#ifdef IO_SYSCALL_WRAPPING
#include "edge_syscall.h"
#endif /*  IO_SYSCALL_WRAPPING */

#include "adm.h"

edgecallwrapper edge_call_table[MAX_EDGE_CALL];
edgecallwrapper protected_edge_call_table[MAX_EDGE_CALL];

/* Registered handler for incoming edge calls */
void
incoming_call_dispatch(void* buffer) {
  struct edge_call* edge_call = (struct edge_call*)buffer;

#ifdef IO_SYSCALL_WRAPPING
  /* If its a syscall handle it specially */
  if (edge_call->call_id == EDGECALL_SYSCALL) {
    incoming_syscall(buffer);
    return;
  }
#endif /*  IO_SYSCALL_WRAPPING */

  /* Otherwise try to lookup the call in the table */
  if (edge_call->call_id > MAX_EDGE_CALL ||
      edge_call_table[edge_call->call_id] == NULL) {
    /* Fatal error */
    goto fatal_error;
  }
  edge_call_table[edge_call->call_id](buffer);
  return;

fatal_error:
  edge_call->return_data.call_status = CALL_STATUS_BAD_CALL_ID;
  return;
}

void
protected_incoming_call_dispatch(void* buffer) {
  struct edge_call* edge_call = (struct edge_call*)buffer;
#ifdef IO_SYSCALL_WRAPPING
  if (edge_call->call_id == EDGECALL_SYSCALL) {
    protected_incoming_syscall(edge_call);
    return;
  }
#endif
  /* Otherwise try to lookup the call in the table */
  if (edge_call->call_id > MAX_EDGE_CALL ||
      protected_edge_call_table[edge_call->call_id] == NULL) {
    /* Fatal error */
    goto fatal_error;
  }
  protected_edge_call_table[edge_call->call_id]((void*)edge_call);
  return;

fatal_error:
  edge_call->return_data.call_status = CALL_STATUS_BAD_CALL_ID;
  return;
}

int
register_call(unsigned long call_id, edgecallwrapper func) {
  if (call_id > MAX_EDGE_CALL) {
    return -1;
  }

  edge_call_table[call_id] = func;
  return 0;
}

int
register_protected_call(unsigned long call_id, edgecallwrapper func) {
  if (call_id > MAX_EDGE_CALL) {
    return -1;
  }

  protected_edge_call_table[call_id] = func;
  return 0;
}
