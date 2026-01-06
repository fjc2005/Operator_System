#include <stdio.h>
#include <monitor.h>
#include <kmalloc.h>
#include <assert.h>


// Initialize monitor.
void     
monitor_init (monitor_t * mtp, size_t num_cv) {
    int i;
    assert(num_cv>0);
    mtp->next_count = 0;
    mtp->cv = NULL;
    sem_init(&(mtp->mutex), 1); //unlocked
    sem_init(&(mtp->next), 0);
    mtp->cv =(condvar_t *) kmalloc(sizeof(condvar_t)*num_cv);
    assert(mtp->cv!=NULL);
    for(i=0; i<num_cv; i++){
        mtp->cv[i].count=0;
        sem_init(&(mtp->cv[i].sem),0);
        mtp->cv[i].owner=mtp;
    }
}

// Free monitor.
void
monitor_free (monitor_t * mtp, size_t num_cv) {
    kfree(mtp->cv);
}

// Unlock one of threads waiting on the condition variable. 
void 
cond_signal (condvar_t *cvp) {
   //LAB7 EXERCISE1: YOUR CODE
   cprintf("cond_signal begin: cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
   /*
   *      cond_signal(cv) {
   *          if(cv.count>0) {
   *             mt.next_count ++;
   *             signal(cv.sem);
   *             wait(mt.next);
   *             mt.next_count--;
   *          }
   *       }
   */
   // 如果有进程在等待这个条件变量
   if (cvp->count > 0) {
       // 增加next_count，表示有一个信号发送者在等待
       cvp->owner->next_count++;
       // 唤醒等待在条件变量上的进程
       up(&(cvp->sem));
       // 信号发送者等待被唤醒的进程执行完成
       down(&(cvp->owner->next));
       // 被唤醒后，减少next_count
       cvp->owner->next_count--;
   }
   cprintf("cond_signal end: cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
}

// Suspend calling thread on a condition variable waiting for condition Atomically unlocks 
// mutex and suspends calling thread on conditional variable after waking up locks mutex. Notice: mp is mutex semaphore for monitor's procedures
void
cond_wait (condvar_t *cvp) {
    //LAB7 EXERCISE1: YOUR CODE
    cprintf("cond_wait begin:  cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
   /*
    *         cv.count ++;
    *         if(mt.next_count>0)
    *            signal(mt.next)
    *         else
    *            signal(mt.mutex);
    *         wait(cv.sem);
    *         cv.count --;
    */
    // 增加等待计数
    cvp->count++;
    // 如果有信号发送者在等待，唤醒它
    if (cvp->owner->next_count > 0) {
        up(&(cvp->owner->next));
    } else {
        // 否则释放管程的互斥锁
        up(&(cvp->owner->mutex));
    }
    // 等待条件变量被信号唤醒
    down(&(cvp->sem));
    // 被唤醒后，减少等待计数
    cvp->count--;
    cprintf("cond_wait end:  cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
}
