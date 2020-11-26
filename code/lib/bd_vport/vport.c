// TODO: add license of VPORT
#include <x86intrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include <rte_malloc.h>

#include "vport.h"
#include "llring_pool.h"
#include "list.h"

#define ROUND_TO_64(x) ((x + 64) & (~0x3f))

void _rand_set_port_mac_address(struct vport *port)
{
  uint32_t i;
  uint32_t seed;
  seed = __rdtsc();
  srand(seed);
  for (i = 0; i < 6; i++) {
    port->mac_addr[i] = rand() & 0xff;
  }
  port->mac_addr[0] &= 0xfe; // not broadcast/multicast
  port->mac_addr[0] |= 0x02; // locally administered
}

/* Connect to an existing vport_bar
 * */
struct vport *from_vport_name(char *port_name)
{
  int res;
  size_t bar_address;
  FILE *fp;
  char port_path[PORT_DIR_LEN];
  res = snprintf(port_path, PORT_DIR_LEN, "%s/%s/%s", TMP_DIR, VPORT_DIR_PREFIX,
                 port_name);
  if (res >= PORT_DIR_LEN) return NULL;

  fp = fopen(port_path, "r");
  if (fp == NULL) return NULL;

  res = fread(&bar_address, 8, 1, fp);
  if (res == 0) return NULL;

  fclose(fp);

  return from_vbar_addr(bar_address);
}

/* Connect to an existing vport_bar
 * */
struct vport *from_vbar_addr(size_t bar_address)
{
  struct vport *port;
  size_t port_size;
  struct vport_bar *bar;
  uint8_t *ptr;

  bar = (struct vport_bar *)bar_address;
  port_size = sizeof(struct vport);
  ptr = rte_zmalloc(NULL, port_size, 0);
  port = (struct vport *)ptr;
  assert(port);
  port->_main = 0; // this is connected to someone elses vport_bar
  port->bar = bar;

  // set port mac address
  _rand_set_port_mac_address(port);
  return port;
}

/* Allocate a new vport and vport_bar
 * Setup pipes
 * */
struct vport *new_vport(const char *name, uint16_t num_inc_q,
                        uint16_t num_out_q)
{
  uint16_t pool_size = (num_inc_q + num_out_q) * 3 / 2;
  if (pool_size < num_inc_q + num_out_q)
    return NULL;
  return _new_vport(name, num_inc_q, num_out_q, SLOTS_PER_LLRING, pool_size);
}

struct vport *_new_vport(const char *name, uint16_t num_inc_q,
                         uint16_t num_out_q, uint16_t pool_size,
                         uint16_t q_size)
{
  uint32_t i, j;

  uint32_t inc_qs_ptr_size;
  uint32_t out_qs_ptr_size;
  size_t total_memory_needed;
  uint32_t port_size;

  uint8_t *ptr;
  struct llr_seg *seg;
  FILE *fp;

  struct stat sb;
  char port_dir[PORT_DIR_LEN];
  char file_name[PORT_DIR_LEN];

  struct vport *port;
  struct vport_bar *bar;
  size_t bar_address;

  struct rate *rate;

  // allocate vport struct
  port_size = sizeof(struct vport);
  ptr = rte_zmalloc(NULL, port_size, 0);
  port = (struct vport *)ptr;
  assert(port != NULL);
  port->_main = 1; // this is the main vport (ownership of vbar)

  // Calculate how much memory is needed for vport_bar
  inc_qs_ptr_size = ROUND_TO_64(sizeof(struct llr_seg *)) * num_inc_q;
  out_qs_ptr_size = ROUND_TO_64(sizeof(struct llr_seg *)) * num_out_q;
  total_memory_needed = ROUND_TO_64(sizeof(struct vport_bar)) +
                        inc_qs_ptr_size + out_qs_ptr_size +
                        ((num_inc_q + num_out_q) * ROUND_TO_64(sizeof(struct rate)));

  bar = rte_zmalloc(NULL, total_memory_needed, 64);
  assert(bar);

  bar_address = (size_t)bar;
  port->bar = bar;

  // Initialize vport_bar structure
  strncpy(bar->name, name, PORT_NAME_LEN);
  bar->num_inc_q = num_inc_q;
  bar->num_out_q = num_out_q;
  bar->th_goal = q_size / 10;
  bar->th_over = (q_size >> 3) * 7; // 87.5%
  bar->pool = new_llr_pool(name, SOCKET_ID_ANY, pool_size, q_size);

  ptr = (uint8_t *)bar;
  ptr += ROUND_TO_64(sizeof(struct vport_bar));
  bar->inc_qs = (struct llr_seg **)ptr;
  ptr += inc_qs_ptr_size;
  bar->out_qs = (struct llr_seg **)(ptr);

  // Set memory for rate arrays
  ptr += out_qs_ptr_size;
  bar->inc_rate = (struct rate *)ptr;
  ptr += (num_inc_q * ROUND_TO_64(sizeof(struct rate)));
  bar->out_rate = (struct rate *)ptr;

  // Init each queue
  for (i = 0; i < num_inc_q; i++) {
    seg = pull_llr(bar->pool);
    assert(seg != NULL);
    INIT_LIST_HEAD(&seg->list);
    // printf("inc assigning qid: %d, ptr: %p next: %p\n",
    //         i, &seg->list, seg->list.next);
    bar->inc_qs[i] = seg;

    rate = &bar->inc_rate[i];
    rate->tail = RATE_SEQUENCE_SIZE - 1;
    rate->sum = RATE_INIT_VALUE * (RATE_SEQUENCE_SIZE - 1);
    rate->pps = RATE_INIT_VALUE;
    for (j = 0; j < RATE_SEQUENCE_SIZE - 1; j++)
      rate->sequence[j] = RATE_INIT_VALUE;
  }

  for (i = 0; i < num_out_q; i++) {
    seg = pull_llr(bar->pool);
    assert(seg != NULL);
    INIT_LIST_HEAD(&seg->list);
    // printf("out assigning qid: %d, ptr: %p next: %p\n",
    //        i, &seg->list, seg->list.next);
    bar->out_qs[i] = seg;

    rate = &bar->out_rate[i];
    rate->tail = RATE_SEQUENCE_SIZE - 1;
    rate->sum = RATE_INIT_VALUE * (RATE_SEQUENCE_SIZE - 1);
    rate->pps = RATE_INIT_VALUE;
    for (j = 0; j < RATE_SEQUENCE_SIZE - 1; j++)
      rate->sequence[j] = RATE_INIT_VALUE;
  }

  // Create temp directory
	snprintf(port_dir, PORT_DIR_LEN, "%s/%s", TMP_DIR, VPORT_DIR_PREFIX);
  if (stat(port_dir, &sb) == 0) {
    assert((sb.st_mode & S_IFMT) == S_IFDIR);
  } else {
    printf("Creating directory %s\n", port_dir);
    mkdir(port_dir, S_IRWXU | S_IRWXG | S_IRWXO);
  }

  // Set port mac address
  _rand_set_port_mac_address(port);

  // Create socket file (file with shared memory information)
  snprintf(file_name, PORT_DIR_LEN, "%s/%s/%s", TMP_DIR,
           VPORT_DIR_PREFIX, name);
  printf("Writing port information to %s\n", file_name);
  fp = fopen(file_name, "w");
  fwrite(&bar_address, 8, 1, fp);
  fclose(fp);
  return port;
}

int free_vport(struct vport *port)
{
  if (port->_main) {
    // TODO: keep track of number of connected vports
    // Make sure there is no other vport connected to this vport bar
    char file_name[PORT_DIR_LEN];
    char *name = port->bar->name;

    snprintf(file_name, PORT_DIR_LEN, "%s/%s/%s", TMP_DIR, VPORT_DIR_PREFIX,
             name);
    unlink(file_name);

    free_llr_pool(port->bar->pool);
    rte_free(port->bar);
  }

  rte_free(port);
  return 0;
}

int send_packets_vport(struct vport *port, uint16_t qid, void**pkts, int cnt)
{
  uint64_t pause_duration;
  return send_packets_vport_with_bp(port, qid, pkts, cnt, &pause_duration);
}

/* Send packet to the queue.
 * pause_duration is in nano-seconds (ns)
 * */
int send_packets_vport_with_bp(struct vport *port, uint16_t qid, void **pkts,
                               int cnt, uint64_t *pause_duration)
{
  unsigned queue_size;
  struct llr_seg *q_list;
  int32_t ret;
  uint64_t pps;

  *pause_duration = 0;

  do {
    if (port->_main) {
      q_list = port->bar->out_qs[qid];
      pps = port->bar->out_rate[qid].pps;
    } else {
      q_list = port->bar->inc_qs[qid];
      pps = port->bar->inc_rate[qid].pps;
    }
    // pps = 12000000;
    // printf("qid: %d pps: %ld\n", qid, pps);
    // get the tail
    q_list = list_entry(q_list->list.prev, struct llr_seg, list);
  } while(q_list == LIST_POISON1 || q_list == LIST_POISON2);

  // ret = llring_enqueue_bulk(q, pkts, cnt);
  ret = llring_enqueue_burst(&q_list->ring, pkts, cnt);
  ret &= 0x7fffffff;

  queue_size = llring_count(&q_list->ring);
  if (queue_size > port->bar->th_over) {
    if (pps > 0) {
      *pause_duration = ((uint64_t)(queue_size - port->bar->th_goal)) * ((1000000000L) / pps);
      // printf("delta packet: %d\n", queue_size - port->bar->th_goal);
      // printf("pause: pps %ld, size: %d, duration: %ld\n", pps, queue_size, *pause_duration);
    } else {
      *pause_duration = 10000;
    }
  }

  return ret;
  // if (ret == -LLRING_ERR_NOBUF)
  //   return 0;

  // if (__sync_bool_compare_and_swap(&port->bar->out_regs[qid]->irq_enabled, 1, 0)) {
  //   char t[1] = {'F'};
  //   ret = write(port->out_irq_fd[qid], (void *)t, 1);
  // }

  // return cnt;
}

int recv_packets_vport(struct vport *port, uint16_t qid, void**pkts, int cnt)
{
  unsigned queue_size;
  struct llr_seg *q_list;
  struct llr_seg *seg;
  int ret;
  struct rate *rate;

  if (port->_main) {
    q_list = port->bar->inc_qs[qid];
    rate = &port->bar->inc_rate[qid];
  } else {
    q_list = port->bar->out_qs[qid];
    rate = &port->bar->out_rate[qid];
  }

  queue_size = llring_count(&q_list->ring);
  if (queue_size > q_list->ring.common.watermark) {
    // printf("\nWe have crossed the water mark\n");
    // water mark crossed
    seg = pull_llr(port->bar->pool);
    list_add_tail(&seg->list, &q_list->list);
    // printf("Extend queue %d q: %p new list: %p next: %p\n\n",
    //     qid, &q_list->list, &seg->list, q_list->list.next);
    // fflush(stdout);
  }

  // TODO: update code to work with bulk
  ret = llring_dequeue_burst(&q_list->ring, pkts, cnt);
  if (queue_size == 0) {
    // check if there is a tail
    if (q_list->list.next != &q_list->list) {
      // printf("qid: %d me: %p, next: %p\n", qid, &q_list->list, q_list->list.next);
      // fflush(stdout);
      // there are some other queues
      if (port->_main) {
        port->bar->inc_qs[qid] = list_entry(q_list->list.next, struct llr_seg, list);
      } else {
        port->bar->out_qs[qid] = list_entry(q_list->list.next, struct llr_seg, list);
      }
      list_del(&q_list->list);
      push_llr(port->bar->pool, q_list);
      // printf("push llring back\n");
    }
  }

  rate->tp += ret;
  uint64_t now = rte_get_timer_cycles();
  if (now - rate->last_ts > rte_get_timer_hz()) {
    rate->sum += rate->tp;
    rate->sequence[rate->tail] = rate->tp;
    rate->tail = (rate->tail + 1) % RATE_SEQUENCE_SIZE;
    // if (rate->tail == rate->head) {
      rate->sum -= rate->sequence[rate->head];
      rate->head = (rate->head + 1) % RATE_SEQUENCE_SIZE;
    // }
    rate->tp = 0;
    rate->last_ts = now;
    rate->pps = rate->sum / (RATE_SEQUENCE_SIZE - 1);
    // printf("main: %d qid: %d pps: %ld\n", port->_main, qid, rate->pps);
  }
  // if (rate->pps > 0)
  //   printf("qid: %d pps: %ld\n", qid, rate->pps);
  return ret;
}
