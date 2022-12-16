#include <linux/if.h>
#include <linux/if_tun.h>
#include <stdio.h>
#include <rte_common.h> 
#include <rte_eal.h> 
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_port_fd.h>

#define TAP_DEV          "/dev/net/tun"
#define NAME_SIZE          64

struct tap {
	TAILQ_ENTRY(tap) node;
	char name[NAME_SIZE];
	int fd;
};

TAILQ_HEAD(tap_list, tap);

static struct tap_list tap_list;


int
tap_init(void)
{
	TAILQ_INIT(&tap_list);

	return 0;
}

struct tap *
tap_find(const char *name)
{
	struct tap *tap;

	if (name == NULL)
		return NULL;

	TAILQ_FOREACH(tap, &tap_list, node)
		if (strcmp(tap->name, name) == 0)
			return tap;

	return NULL;
}

struct tap *
tap_create(const char *name)
{
	struct tap *tap;
	struct ifreq ifr;
	int fd, status;

	/* Check input params */
	if ((name == NULL) ||
		tap_find(name))
		return NULL;

	/* Resource create */
	fd = open(TAP_DEV, O_RDWR | O_NONBLOCK);
	if (fd < 0)
		return NULL;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI; /* No packet information */
	strlcpy(ifr.ifr_name, name, IFNAMSIZ);

	status = ioctl(fd, TUNSETIFF, (void *) &ifr);
	if (status < 0) {
		close(fd);
		return NULL;
	}

	/* Node allocation */
	tap = calloc(1, sizeof(struct tap));
	if (tap == NULL) {
		close(fd);
		return NULL;
	}
	/* Node fill in */
	strlcpy(tap->name, name, sizeof(tap->name));
	tap->fd = fd;

	/* Node add to list */
	TAILQ_INSERT_TAIL(&tap_list, tap, node);

	return tap;
}

#define POOL_SIZE           (32 * 1024)
#define POOL_CACHE_SIZE     256
#define POOL_BUFFER_SIZE    RTE_MBUF_DEFAULT_BUF_SIZE


static void
app_init_mbuf_pools(char *poolname, struct rte_mempool **pool)
{
	/* Init the buffer pool */
	printf("Getting/Creating the mempool ...\n");

    *pool = rte_pktmbuf_pool_create(
        poolname,
        POOL_SIZE,
        POOL_CACHE_SIZE, 0, POOL_BUFFER_SIZE,
        0);

    if (pool == NULL)
        rte_panic("Cannot create mbuf pool\n");
}

#define TAP_NAME_RX "tap0_rx"
struct rte_port_in_ops *g_rx_port_ops;
struct rte_port_fd_writer *g_rx_port;

#define TAP_NAME_TX "tap0_tx"
struct rte_port_out_ops *g_tx_port_ops;
struct rte_port_fd_reader *g_tx_port;


int port_rx_init(char *tap_name)
{
    struct rte_port_fd_reader_params params = {0};
    struct tap *ptap = NULL;

    ptap = tap_find(tap_name);
    if (NULL == ptap){
        printf("not find tap %s\n", tap_name);
        return -1;
    }

    params.fd = ptap->fd;
    params.mtu = 1500;

    app_init_mbuf_pools(tap_name, &params.mempool);

    g_rx_port_ops = &rte_port_fd_reader_ops;
    g_rx_port = g_rx_port_ops->f_create(&params, 0);

    return 0;
}


int port_tx_init(char *tap_name)
{
    struct rte_port_fd_writer_params params = {0};
    struct tap *ptap = NULL;

    ptap = tap_find(tap_name);
    if (NULL == ptap){
        printf("not find tap %s\n", tap_name);
        return -1;
    }

    params.fd = ptap->fd;
    params.tx_burst_sz = 1;

    g_tx_port_ops = &rte_port_fd_writer_ops;
    g_tx_port = g_tx_port_ops->f_create(&params, 0);

    return 0;
}


int main_port_init(void)
{
    tap_create(TAP_NAME_RX);
    port_rx_init(TAP_NAME_RX);

    tap_create(TAP_NAME_TX);
    port_tx_init(TAP_NAME_TX);

    return 0;
}


int rcv_and_snd_pkts()
{
    uint32_t n_pkts;
    struct rte_mbuf *pkts[64] = {0};

    /* Input port RX */
    n_pkts = g_rx_port_ops->f_rx(g_rx_port, pkts,
		64);

    printf("rcv num pkts %d\n", n_pkts);
    g_tx_port_ops->f_tx(g_tx_port, pkts[0]);
    
    //for (i = 0; i < n_pkts; i ++)
    //    rte_pktmbuf_free(pkts[i]);

    return 0;
}

int main(int argc, char **argv)
{
    int ret = 0;

    ret = rte_eal_init(argc, argv);
    if (0 != ret){
        printf("rte_eal_init failed ret %d\n", ret);
        return -1;
    }

    tap_init();

    printf("tap create\n");
    main_port_init();
    printf("port init\n");
    for (; ;){
        printf("start rcv pkts");
        rcv_and_snd_pkts();
        sleep(1);
    }

    return 0;

}
