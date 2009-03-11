/*
 * Copyright (C) 2008 Search Solution Corporation. All rights reserved by Search Solution.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or 
 *   (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

/*
 * replication.c -
 */

#ident "$Id$"

#include <stdio.h>
#include <assert.h>
#include "replication.h"
#include "object_primitive.h"
#include "heap_file.h"

/*
 * EXTERN TO ALL SERVER RECOVERY FUNCTION CODED SOMEWHERE ELSE
 */

#define 	REPL_LOG_IS_NOT_EXISTS(tran_index)                            \
               (log_Gl.trantable.all_tdes[(tran_index)]->num_repl_records == 0)
#define 	REPL_LOG_IS_FULL(tran_index)                                  \
               (log_Gl.trantable.all_tdes[(tran_index)]->num_repl_records     \
                 == log_Gl.trantable.all_tdes[(tran_index)]->cur_repl_record+1)

static const int REPL_LOG_INFO_ALLOC_SIZE = 100;

#if defined(SERVER_MODE) || defined(SA_MODE)
static int repl_log_info_alloc (LOG_TDES * tdes, int arr_size,
				bool need_realloc);
#if !defined(WINDOWS)
static REPL_SAVEPOINT_INFO *repl_alloc_savepoint_info (const char *sp_name);
#endif
#endif /* SERVER_MODE || SA_MODE */

#if defined(SERVER_MODE) || defined(SA_MODE)

/*
 * repl_data_insert_log_dump - dump the "DATA INSERT" replication log
 *
 * return:
 *
 *   length(in): length of the data
 *   data(in): log data
 *
 * NOTE:
 */
void
repl_data_insert_log_dump (int length, void *data)
{
  char *class_name;
  DB_VALUE key;
  char *ptr;

  ptr = or_unpack_string_nocopy ((char *) data, &class_name);
  ptr = or_unpack_value (ptr, &key);
  fprintf (stdout, "      class_name: %s\n", class_name);
  fprintf (stdout, "      pk_value: ");
  db_value_print (&key);
  pr_clear_value (&key);
  fprintf (stdout, "\n");
  fflush (stdout);
}

/*
 * repl_data_udpate_log_dump - dump the "DATA UPDATE" replication log
 *
 * return:
 *
 *   length(in): length of the data
 *   data(in): log data
 *
 * NOTE:
 */
void
repl_data_udpate_log_dump (int length, void *data)
{
  /* currently same logic as insert case, but I can't gaurantee it's true
   * after this...
   */
  repl_data_insert_log_dump (length, data);
}

/*
 * repl_data_delete_log_dump - dump the "DATA DELETE" replication log
 *
 * return:
 *
 *   length(in): length of the data
 *   data(in): log data
 *
 * NOTE:
 */
void
repl_data_delete_log_dump (int length, void *data)
{
  /* currently same logic as insert case, but I can't gaurantee it's true
   * after this...
   */
  repl_data_insert_log_dump (length, data);
}

/*
 * repl_schema_log_dump -
 *
 * return:
 *
 *   length(in):
 *   data(in):
 *
 * NOTE:
 */
void
repl_schema_log_dump (int length, void *data)
{
  int type;
  char *class_name, *ddl;
  char *ptr;

  ptr = or_unpack_int ((char *) data, &type);
  ptr = or_unpack_string_nocopy (ptr, &class_name);
  ptr = or_unpack_string_nocopy (ptr, &ddl);
  fprintf (stdout, "      type: %d\n", type);
  fprintf (stdout, "      class_name: %s\n", class_name);
  fprintf (stdout, "      DDL: %s\n", ddl);
  fflush (stdout);
}

/*
 * repl_class_is_replicated - check the target class is set for replication
 *
 * return: true if replicated class, false otherwise
 *
 *   class_oid(in): OID of the target class
 *
 * NOTE:The class attribute "is_replicated" of the target class shows that
 *     this class should be replicated or not.
 *
 *     The class can be replicated when it has a primary key.
 *
 *     if databases is set for replication AND the target class is set for
 *     replication, then the replication log would be append when the
 *     transaction commits.
 */
bool
repl_class_is_replicated (OID * class_oid)
{
  if (OID_ISNULL (class_oid))
    return false;
  else				/* We have to check is_replicated attr. of _db_class */
    return true;
}

/*
 * repl_log_info_alloc - log info area allocation
 *
 * return: Error Code
 *
 *   tdes(in): transaction descriptor
 *   arr_size(in): array size to be allocated
 *   need_realloc(in): 0-> initial allocation (malloc), otherwise "realloc"
 *
 * NOTE:This function allocates the memory for the log info area of the
 *       target transaction. It is called when the transaction tries to to
 *   insert/update/delete operation. If the transaction do read operation
 *   only, no memory is allocated.
 *
 *   The allocation size is defined by a constant - REPL_LOG_INFO_ALLOC_SIZE
 *   We need to set the size for the user request ?
 */
static int
repl_log_info_alloc (LOG_TDES * tdes, int arr_size, bool need_realloc)
{
  int i = 0, k;
  int error = NO_ERROR;

  if (need_realloc == false)
    {
      i = arr_size * DB_SIZEOF (LOG_REPL_RECORD);
      tdes->repl_records = (LOG_REPL_RECORD *) malloc (i);
      if (tdes->repl_records == NULL)
	{
	  REPL_ERROR (error, "can't allocate memory");
	  return error;
	}
      tdes->num_repl_records = arr_size;
      k = 0;
    }
  else
    {
      i = tdes->num_repl_records + arr_size;
      tdes->repl_records = (LOG_REPL_RECORD *)
	realloc (tdes->repl_records, i * DB_SIZEOF (LOG_REPL_RECORD));
      if (tdes->repl_records == NULL)
	{
	  REPL_ERROR (error, "can't allocate memory");
	  return error;
	}
      k = tdes->num_repl_records;
      tdes->num_repl_records = i;
    }

  for (i = k; i < tdes->num_repl_records; i++)
    {
      tdes->repl_records[i].repl_data = NULL;
    }

  return error;
}

/*
 * repl_add_update_lsa - update the LSA of the target transaction lo
 *
 * return: NO_ERROR or error code
 *
 *   inst_oid(in): OID of the instance
 *
 * NOTE:For update operation, the server does "Heap stuff" after "Index Stuff".
 *     In order to reduce the cost of replication log, we generates a
 *     replication log at the point of indexing  processing step.
 *     (During index processing, the primary key value is fetched ..)
 *     After Heap operation, we have to set the target LSA into the
 *     replication log.
 *
 *     For update case, this function is called by locator_update_force().
 *     In the case of insert/delete cases, when the replication log info. is
 *     generated, the server already has the target transaction log(HEAP_INSERT
 *     or HEAP_DELETE).
 *     But, for the udpate case, the server doesn't has the target log when
 *     it generates the replication log. So, the server has to find out the
 *     location of replication record and match with the target transaction
 *     log after heap_update(). This is done by locator_update_force().
 */
int
repl_add_update_lsa (THREAD_ENTRY * thread_p, OID * inst_oid)
{
  int tran_index;
  LOG_TDES *tdes;
  LOG_REPL_RECORD *repl_rec;
  int i;
  bool find = false;
  int error = NO_ERROR;

  tran_index = LOG_FIND_THREAD_TRAN_INDEX (thread_p);

  tdes = LOG_FIND_TDES (tran_index);

  for (i = tdes->cur_repl_record - 1; i >= 0; i--)
    {
      repl_rec = (LOG_REPL_RECORD *) & tdes->repl_records[i];
      if (OID_EQ (&repl_rec->inst_oid, inst_oid)
	  && !LSA_ISNULL (&tdes->repl_update_lsa))
	{
	  LSA_COPY (&repl_rec->lsa, &tdes->repl_update_lsa);
	  LSA_SET_NULL (&tdes->repl_update_lsa);
	  LSA_SET_NULL (&tdes->repl_insert_lsa);
	  find = true;
	  break;
	}
    }

  if (find == false)
    er_log_debug (ARG_FILE_LINE, "can't find out the UPDATE LSA");

  return error;
}

/*
 * repl_log_insert - insert a replication info to the transaction descriptor
 *
 * return: NO_ERROR or error code
 *
 *   class_oid(in): OID of the class
 *   inst_oid(in): OID of the instance
 *   log_type(in): log type (DATA or SCHEMA)
 *   rcvindex(in): recovery index (INSERT or DELETE or UPDATE)
 *   key_dbvalue(in): Primary Key value
 *
 * NOTE:insert a replication log info to the transaction descriptor (tdes)
 */
int
repl_log_insert (THREAD_ENTRY * thread_p, OID * class_oid,
		 OID * inst_oid,
		 LOG_RECTYPE log_type,
		 LOG_RCVINDEX rcvindex, DB_VALUE * key_dbvalue)
{
  int tran_index;
  LOG_TDES *tdes;
  LOG_REPL_RECORD *repl_rec;
  char *class_name;
  char *ptr;
  int error = NO_ERROR;

  tran_index = LOG_FIND_THREAD_TRAN_INDEX (thread_p);

  tdes = LOG_FIND_TDES (tran_index);

  /* check the replication log array status, if we need to alloc? */
  if (REPL_LOG_IS_NOT_EXISTS (tran_index)
      && ((error = repl_log_info_alloc (tdes, REPL_LOG_INFO_ALLOC_SIZE,
					false)) != NO_ERROR))
    {
      return error;
    }
  /* the replication log array is full? re-alloc? */
  else if (REPL_LOG_IS_FULL (tran_index)
	   && (error = repl_log_info_alloc (tdes, REPL_LOG_INFO_ALLOC_SIZE,
					    true)) != NO_ERROR)
    {
      return error;
    }

  repl_rec = (LOG_REPL_RECORD *) & tdes->repl_records[tdes->cur_repl_record];
  repl_rec->repl_type = log_type;
  COPY_OID (&repl_rec->inst_oid, inst_oid);

  /* make the common info for the data replication */
  if (log_type == LOG_REPLICATION_DATA)
    {
      repl_rec->length = or_db_value_size (key_dbvalue);

      class_name = heap_get_class_name (thread_p, (const OID *) class_oid);
      repl_rec->length += or_packed_string_length (class_name);

      if ((repl_rec->repl_data = (char *) malloc (repl_rec->length)) == NULL)
	{
	  REPL_ERROR (error, "can't allocate memory");
	  return error;
	}
      ptr = repl_rec->repl_data;
      ptr = or_pack_string (ptr, class_name);
      ptr = or_pack_value (ptr, key_dbvalue);
      db_value_clear (key_dbvalue);
      free_and_init (class_name);

    }
  else
    {
      repl_rec->repl_data = NULL;
      repl_rec->length = 0;
    }
  repl_rec->must_flush = LOG_REPL_COMMIT_NEED_FLUSH;

  switch (rcvindex)
    {
    case RVREPL_DATA_INSERT:
      if (!LSA_ISNULL (&tdes->repl_insert_lsa))
	{
	  LSA_COPY (&repl_rec->lsa, &tdes->repl_insert_lsa);
	  LSA_SET_NULL (&tdes->repl_insert_lsa);
	  LSA_SET_NULL (&tdes->repl_update_lsa);
	}
      break;
    case RVREPL_DATA_UPDATE:
      /*
       * for the update case, this function is called before the heap
       * file update, so we don't need to LSA for update log here.
       */
      LSA_SET_NULL (&repl_rec->lsa);
      break;
    case RVREPL_DATA_DELETE:
      /*
       * for the delete case, we don't need to find out the target
       * LSA. Delete is operation is possible without "After Image"
       */
      LSA_SET_NULL (&repl_rec->lsa);
      break;
    default:
      break;
    }
  tdes->cur_repl_record++;

  /* if flush marking is started, mark "must_flush" at current log
   * except the log conflicts with previous logs due to same instance update
   */
  if (tdes->fl_mark_repl_recidx != -1)
    {
      LOG_REPL_RECORD *recsp = tdes->repl_records;
      int i;

      for (i = 0; i < tdes->fl_mark_repl_recidx; i++)
	{
	  if (recsp[i].must_flush == LOG_REPL_COMMIT_NEED_FLUSH
	      && OID_EQ (&recsp[i].inst_oid, &repl_rec->inst_oid))
	    {
	      break;
	    }
	}

      if (i >= tdes->fl_mark_repl_recidx)
	{
	  repl_rec->must_flush = LOG_REPL_NEED_FLUSH;
	}
    }

  return error;
}

/*
 * repl_log_insert_schema - insert a replication info(schema) to the
 *                          transaction descriptor
 *
 * return: NO_ERROR or error code
 *
 *   repl_schema(in):
 *
 * NOTE:insert a replication log info(schema) to the transaction
 *      descriptor (tdes)
 */
int
repl_log_insert_schema (THREAD_ENTRY * thread_p,
			REPL_INFO_SCHEMA * repl_schema)
{
  int tran_index;
  LOG_TDES *tdes;
  LOG_REPL_RECORD *repl_rec;
  char *ptr;
  int error = NO_ERROR;

  tran_index = LOG_FIND_THREAD_TRAN_INDEX (thread_p);

  tdes = LOG_FIND_TDES (tran_index);

  /* check the replication log array status, if we need to alloc? */
  if (REPL_LOG_IS_NOT_EXISTS (tran_index)
      && ((error = repl_log_info_alloc (tdes, REPL_LOG_INFO_ALLOC_SIZE,
					false)) != NO_ERROR))
    {
      return error;
    }
  /* the replication log array is full? re-alloc? */
  else if (REPL_LOG_IS_FULL (tran_index)
	   && (error = repl_log_info_alloc (tdes, REPL_LOG_INFO_ALLOC_SIZE,
					    true)) != NO_ERROR)
    {
      return error;
    }

  repl_rec = (LOG_REPL_RECORD *) & tdes->repl_records[tdes->cur_repl_record];
  repl_rec->repl_type = LOG_REPLICATION_SCHEMA;
  repl_rec->must_flush = LOG_REPL_COMMIT_NEED_FLUSH;
  OID_SET_NULL (&repl_rec->inst_oid);

  /* make the common info for the schema replication */
  repl_rec->length = OR_INT_SIZE;	/* REPL_INFO_SCHEMA.statement_type */
  repl_rec->length += or_packed_string_length (repl_schema->name);
  repl_rec->length += or_packed_string_length (repl_schema->ddl);
  if ((repl_rec->repl_data = (char *) malloc (repl_rec->length)) == NULL)
    {
      REPL_ERROR (error, "can't allocate memory");
      return error;
    }
  ptr = repl_rec->repl_data;
  ptr = or_pack_int (ptr, repl_schema->statement_type);
  ptr = or_pack_string (ptr, repl_schema->name);
  ptr = or_pack_string (ptr, repl_schema->ddl);

  LSA_SET_NULL (&repl_rec->lsa);

  tdes->cur_repl_record++;

  return error;
}

/*
 * repl_start_flush_mark -
 *
 * return:
 *
 * NOTE:start to mark "must_flush" for repl records to be appended after this
 */
void
repl_start_flush_mark (THREAD_ENTRY * thread_p)
{
  LOG_TDES *tdes;

  tdes = LOG_FIND_CURRENT_TDES (thread_p);

  if (tdes == NULL)
    {
      er_set (ER_FATAL_ERROR_SEVERITY, ARG_FILE_LINE,
	      ER_LOG_UNKNOWN_TRANINDEX, 1,
	      LOG_FIND_THREAD_TRAN_INDEX (thread_p));
      return;
    }
  if (tdes->fl_mark_repl_recidx == -1)
    {
      tdes->fl_mark_repl_recidx = tdes->cur_repl_record;
    }				/* else already started, return */
  return;
}

/*
 * repl_end_flush_mark -
 *
 * return:
 *
 *   need_undo(in):
 *
 * NOTE:end to mark "must_flush" for repl records
 */
void
repl_end_flush_mark (THREAD_ENTRY * thread_p, bool need_undo)
{
  LOG_TDES *tdes;
  int i;

  tdes = LOG_FIND_CURRENT_TDES (thread_p);

  if (tdes == NULL)
    {
      er_set (ER_FATAL_ERROR_SEVERITY, ARG_FILE_LINE,
	      ER_LOG_UNKNOWN_TRANINDEX, 1,
	      LOG_FIND_THREAD_TRAN_INDEX (thread_p));
      return;
    }
  if (need_undo)
    {
      LOG_REPL_RECORD *recsp = tdes->repl_records;
      for (i = tdes->fl_mark_repl_recidx; i < tdes->cur_repl_record; i++)
	{
	  /* initialize repl records to be marked as flush */
	  free_and_init (recsp[i].repl_data);
	  recsp[i].repl_data = NULL;
	}
      tdes->cur_repl_record = tdes->fl_mark_repl_recidx;
    }
  tdes->fl_mark_repl_recidx = -1;

  return;
}

#if !defined(WINDOWS)
/*
 * repl_alloc_savepoint_info -
 *
 * return:
 *
 *   sp_name(in): save point name
 *
 */
static REPL_SAVEPOINT_INFO *
repl_alloc_savepoint_info (const char *sp_name)
{
  REPL_SAVEPOINT_INFO *sp_info =
    (REPL_SAVEPOINT_INFO *) malloc (sizeof (REPL_SAVEPOINT_INFO));

  if (sp_info)
    {
      sp_info->sp_name = strdup (sp_name);
      if (sp_info->sp_name == NULL)
	{
	  free (sp_info);
	  return NULL;
	}
      sp_info->log_rec_start_idx = 0;
      sp_info->next = NULL;
    }

  return sp_info;
}

/*
 * repl_free_savepoint_info -
 *
 * return:
 *
 *   node (in) :
 *
 */
void
repl_free_savepoint_info (REPL_SAVEPOINT_INFO * node)
{
  REPL_SAVEPOINT_INFO *next_node;

  while (node)
    {
      next_node = node->next;

      if (node->sp_name)
	{
	  free (node->sp_name);
	}
      free_and_init (node);

      node = next_node;
    }
}

/*
 * repl_add_savepoint_info -
 *
 * return:
 *
 *   thread_p (in) :
 *   sp_name (in) :
 *
 */
int
repl_add_savepoint_info (THREAD_ENTRY * thread_p, const char *sp_name)
{
  int error;
  LOG_TDES *tdes;
  REPL_SAVEPOINT_INFO *new_sp_info, *node, *prev_node;

  tdes = LOG_FIND_CURRENT_TDES (thread_p);

  if (tdes == NULL)
    {
      er_set (ER_FATAL_ERROR_SEVERITY, ARG_FILE_LINE,
	      ER_LOG_UNKNOWN_TRANINDEX, 1,
	      LOG_FIND_THREAD_TRAN_INDEX (thread_p));
      return ER_LOG_UNKNOWN_TRANINDEX;
    }

  /* check sp_name already exist in the savepoint list */
  new_sp_info = prev_node = NULL;
  for (node = tdes->savepoint_list.head; node; node = node->next)
    {
      if (strcmp (node->sp_name, sp_name) == 0)
	{
	  if (prev_node)
	    {
	      prev_node->next = node->next;
	    }
	  else
	    {
	      tdes->savepoint_list.head = node->next;
	    }

	  node->next = NULL;
	  new_sp_info = node;
	  break;
	}
      prev_node = node;
    }

  if (new_sp_info == NULL)
    {
      new_sp_info = repl_alloc_savepoint_info (sp_name);

      if (new_sp_info == NULL)
	{
	  REPL_ERROR (error, "can't allocate memory");
	  return error;
	}
    }

  new_sp_info->log_rec_start_idx = tdes->cur_repl_record + 1;

  if (tdes->savepoint_list.head == NULL)
    {
      tdes->savepoint_list.head = new_sp_info;
    }
  else
    {
      tdes->savepoint_list.tail->next = new_sp_info;
    }

  tdes->savepoint_list.tail = new_sp_info;
  return NO_ERROR;
}

/*
 * repl_log_abort_to_savepoint -
 *
 * return:
 *
 *   thread_p (in) :
 *   sp_name (in) :
 *
 */
int
repl_log_abort_to_savepoint (THREAD_ENTRY * thread_p, const char *sp_name)
{
  LOG_TDES *tdes;
  REPL_SAVEPOINT_INFO *sp_info, *prev_sp_info;
  LOG_REPL_RECORD *repl_rec_arr;
  int start_idx, i, error;

  tdes = LOG_FIND_CURRENT_TDES (thread_p);

  if (tdes == NULL)
    {
      er_set (ER_FATAL_ERROR_SEVERITY, ARG_FILE_LINE,
	      ER_LOG_UNKNOWN_TRANINDEX, 1,
	      LOG_FIND_THREAD_TRAN_INDEX (thread_p));
      return ER_LOG_UNKNOWN_TRANINDEX;
    }

  prev_sp_info = NULL;

  for (sp_info = tdes->savepoint_list.head; sp_info; sp_info = sp_info->next)
    {
      if (strcmp (sp_info->sp_name, sp_name) == 0)
	{
	  start_idx = sp_info->log_rec_start_idx;
	  repl_free_savepoint_info (sp_info);
	  if (prev_sp_info)
	    {
	      prev_sp_info->next = NULL;
	    }
	  else
	    {
	      tdes->savepoint_list.head = NULL;
	    }
	  tdes->savepoint_list.tail = prev_sp_info;

	  break;
	}
      prev_sp_info = sp_info;
    }

  if (sp_info == NULL)
    {
      REPL_ERROR (error, "can't find savepoint name");
      return error;
    }

  if (start_idx <= tdes->cur_repl_record)
    {
      repl_rec_arr = tdes->repl_records;
      for (i = start_idx; i <= tdes->cur_repl_record; i++)
	{
	  repl_rec_arr[i - 1].must_flush = LOG_REPL_DONT_NEED_FLUSH;
	}
    }

  return NO_ERROR;
}
#endif

#if defined(CUBRID_DEBUG)
/*
 * repl_debug_info - DEBUGGING Function, print out the replication log info.
 *
 * return:
 *
 * NOTE:Dump the replication log info to the stdout.
 */
void
repl_debug_info ()
{
  LOG_TDES *tdes;
  LOG_REPL_RECORD *repl_rec;
  int tnum, rnum;
  char *class_name;
  DB_VALUE key;
  char *ptr;

  fprintf (stdout, "REPLICATION LOG DUMP -- Memory\n");
  for (tnum = 0; tnum < log_Gl.trantable.num_assigned_indices; tnum++)
    {
      fprintf (stdout, "*************************************************\n");
      tdes = log_Gl.trantable.all_tdes[tnum];
      fprintf (stdout, "For the Trid : %d\n", (int) tdes->trid);
      for (rnum = 0; rnum < tdes->cur_repl_record; rnum++)
	{
	  fprintf (stdout, "   RECORD # %d\n", rnum);
	  repl_rec = (LOG_REPL_RECORD *) & tdes->repl_records[rnum];
	  fprintf (stdout, "      type: %s\n",
		   log_to_string (repl_rec->repl_type));
	  fprintf (stdout, "      OID: %d - %d - %d\n",
		   repl_rec->inst_oid.volid,
		   repl_rec->inst_oid.pageid, repl_rec->inst_oid.slotid);
	  ptr = or_unpack_string_nocopy (repl_rec->repl_data, &class_name);
	  ptr = or_unpack_value (ptr, &key);
	  fprintf (stdout, "      class_name: %s\n", class_name);
	  fprintf (stdout, "      LSA: %d | %d\n", repl_rec->lsa.pageid,
		   repl_rec->lsa.offset);
	  db_value_print (&key);
	  fprintf (stdout,
		   "\n----------------------------------------------\n");
	  pr_clear_value (&key);
	}
    }
  fflush (stdout);
}
#endif /* CUBRID_DEBUG */

#endif /* SERVER_MODE || SA_MODE */
