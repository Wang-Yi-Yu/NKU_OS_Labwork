/**
 * @file   rcu.h
 * @brief  Single-hart quiescent-state-based reclamation (lab7).
 *
 * A reader brackets a read-side section with rcu_read_lock() and rcu_read_unlock(). A writer
 * publishes a new pointer, then synchronize_rcu() waits until every other process has passed a
 * quiescent state: read-side depth 0, and either the process is sleeping or it was switched out.
 * The old object may be reused only after synchronize_rcu() returns. There is no callback queue
 * and no per-CPU rcu_data.
 */
#ifndef __KERN_SYNC_RCU_H__
#define __KERN_SYNC_RCU_H__

struct proc_struct;

void rcu_read_lock(void);
void rcu_read_unlock(void);
void rcu_note_context_switch(struct proc_struct *prev);
void synchronize_rcu(void);
void rcu_check(void);

#endif /* !__KERN_SYNC_RCU_H__ */
