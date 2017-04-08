/* !!! COMPILE THIS FILE ONLY WITH -ansi !!!
 * CC flag. otherwise, signal handler
 * can`t break getchar() */

#include "boomlib.h"

/** initialization for sighandler,
(it simple to see when smth broken )*/
volatile size_t screen_w = 50;
volatile size_t screen_h = 20;
/** did sig_winch called */
struct bm_client_data data;

int net_port = 0;
char* net_host_name = NULL;
int net_socket = 0;
struct hostent *net_host = NULL;
struct sockaddr_in net_server_address;


struct bm_net_msg net_msg;
struct termios term_old_attributes, term_new_attributes;
volatile int exit_code = 0;
volatile int team_keeper = 0;

void sigint_handler(int sig) {
    (void)sig; /* like-a-yandex: this hide warning*/
    fflush(stdout);
    close(net_socket);
    tcsetattr(0, TCSANOW, &term_old_attributes);
    exit(exit_code);
}

void set_screen_size()
{
    struct winsize wins;
    ioctl(1, TIOCGWINSZ, &wins);
    screen_w = wins.ws_col;
    screen_h = wins.ws_row;
}

void term_resize_processing()
{
    set_screen_size();
    if (signal(SIGWINCH, term_resize_processing) == SIG_ERR) {
        printf("signal(SIGWINCH,... failed\n");
        exit_code = 7;
        sigint_handler(SIGINT);
    }
}

void net_send_msg() {
    write(net_socket, &net_msg, sizeof(struct bm_net_msg));
}

void net_send_flag(int v) {
    net_msg.data[0] = v;
    net_send_msg();
}

void net_send_gameact(char v) {
    net_msg.data[0] = BM_NET_MSG_GAMEACT;
    net_msg.data[1] = v;
    net_send_msg();
}

int run_terminal_client()
{
    int term_queue[3];
    struct pollfd poll_arg[2];
    struct bm_net_msg net_in;
    size_t readed = 0;

    poll_arg[0].fd = 0;
    poll_arg[1].fd = net_socket;
    poll_arg[0].revents = 0;
    poll_arg[1].revents = 0;

    poll_arg[0].events = POLLIN | POLLERR | POLLHUP;
    poll_arg[1].events = POLLIN | POLLERR | POLLHUP;

    if(!isatty(0)) {
        printf("It seems that you reassign output not to term.\n");
        printf("I can`t work in that settings... \n");
        exit_code = 3;
        return 2;
    }

    tcgetattr(0, &term_old_attributes);
    memcpy(&term_new_attributes, &term_old_attributes, sizeof(struct termios));
    term_new_attributes.c_lflag &= ~ECHO;
    term_new_attributes.c_lflag &= ~ICANON;
    /*term_new_attributes.c_cc[VMIN] = 1;*/
    tcsetattr(0, TCSANOW, &term_new_attributes);

    term_resize_processing();
    printf("waiting for connection...\n\n");

    while (1) {
        poll_arg[0].revents = 0;
        poll_arg[1].revents = 0;
        poll(poll_arg, 2, -1);

        if (poll_arg[0].revents & POLLIN) {
            term_queue[0] = 0;
            if (read(0, &term_queue[0], 1))
            switch (term_queue[0]) {
            case 65: /* up */
                if ((term_queue[1] = 91) && (term_queue[2] = 27)) {
                    net_send_gameact('U');
                }
                break;
            case 66: /* down */
                if ((term_queue[1] = 91) && (term_queue[2] = 27)) {
                    net_send_gameact('D');
                }
                break;
            case 67: /* right */
                if ((term_queue[1] = 91) && (term_queue[2] = 27)) {
                    net_send_gameact('R');
                }
                break;
            case 68: /* left */
                if ((term_queue[1] = 91) && (term_queue[2] = 27)) {
                    net_send_gameact('L');
                }
                break;
            /** battle boom */
            case '1':
            /** mine */
            case '2':
            /** medicine */
            case '3':
                net_send_gameact(term_queue[0]);
                break;
            case 'q':
            case '\004':
                printf("exiting....\n");
                return 0;
                break;
            }
            term_queue[2] = term_queue[1];
            term_queue[1] = term_queue[0];
        } else {
            if (poll_arg[0].revents != 0) {
                printf("stdout error\n");
                exit_code = 4;
                sigint_handler(SIGINT);
            }
        }

        if (poll_arg[1].revents & POLLIN) {
            readed += read(net_socket, &net_in + readed,
                           sizeof(struct bm_net_msg) - readed);
        } else {
            if (poll_arg[1].revents != 0) {
                printf("socket error\n");
                exit_code = 5;
                sigint_handler(SIGINT);
            }
        }

        if (readed == sizeof(struct bm_net_msg)) {
            readed = 0;
            switch (net_in.data[0]) {
            case BM_NET_MSG_CDATA:
                bm_net_msg_to_client_data(&net_in, &data);
                bm_draw(&data, screen_h, screen_w);
                break;
            case BM_NET_MSG_DIE:
                net_in.data[BM_MSG_LEN - 1] = 0;
                printf("BN_NET_MSG_DIE reached.\n boom panic. message:\n%s\n",
                       &(net_in.data[1]));
                exit_code = 6;
                sigint_handler(SIGINT);
                break;
            default:
                printf("undefined bn_net_msg reaced. \n boom panic. id:\n%s\n",
                       &(net_in.data[0]));
                exit_code = 7;
                sigint_handler(SIGINT);
                break;
            }
        }

    }
    return 0;
}

int run_terminal_team_keeper()
{
    int term_queue[3];
    struct pollfd poll_arg[2];
    struct bm_net_msg net_in;
    size_t readed = 0;

    poll_arg[0].fd = 0;
    poll_arg[1].fd = net_socket;
    poll_arg[0].revents = 0;
    poll_arg[1].revents = 0;

    poll_arg[0].events = POLLIN | POLLERR | POLLHUP;
    poll_arg[1].events = POLLIN | POLLERR | POLLHUP;

    if(!isatty(0)) {
        printf("It seems that you reassign output not to term.\n");
        printf("I can`t work in that settings... \n");
        exit_code = 3;
        return 2;
    }

    tcgetattr(0, &term_old_attributes);
    memcpy(&term_new_attributes, &term_old_attributes, sizeof(struct termios));
    term_new_attributes.c_lflag &= ~ECHO;
    term_new_attributes.c_lflag &= ~ICANON;
    /*term_new_attributes.c_cc[VMIN] = 1;*/
    tcsetattr(0, TCSANOW, &term_new_attributes);

    term_resize_processing();
    printf("waiting for connection...\n\n");

    while (1) {
        poll_arg[0].revents = 0;
        poll_arg[1].revents = 0;
        poll(poll_arg, 2, -1);

        if (poll_arg[0].revents & POLLIN) {
            term_queue[0] = 0;
            if (read(0, &term_queue[0], 1))
            switch (term_queue[0]) {
            case 's':
                printf("STARTING....\n");
                net_send_flag(BM_NET_MSG_STARTGAME);
                break;
            case 'q':
            case '\004':
                printf("exiting....\n");
                return 0;
                break;
            }
            term_queue[2] = term_queue[1];
            term_queue[1] = term_queue[0];
        } else {
            if (poll_arg[0].revents != 0) {
                printf("stdout error\n");
                exit_code = 4;
                sigint_handler(SIGINT);
            }
        }

        if (poll_arg[1].revents & POLLIN) {
            readed += read(net_socket, &net_in + readed,
                           sizeof(struct bm_net_msg) - readed);
        } else {
            if (poll_arg[1].revents != 0) {
                printf("socket error\n");
                exit_code = 5;
                sigint_handler(SIGINT);
            }
        }

        if (readed == sizeof(struct bm_net_msg)) {
            readed = 0;
            switch (net_in.data[0]) {
            case BM_NET_MSG_CDATA:
                bm_draw_raw_net(&net_in);
                break;
            case BM_NET_MSG_DIE:
                net_in.data[BM_MSG_LEN - 1] = 0;
                printf("BN_NET_MSG_DIE reached.\n boom panic. message:\n%s\n",
                       &(net_in.data[1]));
                exit_code = 6;
                sigint_handler(SIGINT);
                break;
            default:
                printf("undefined bn_net_msg reaced. \n boom panic. id:\n%s\n",
                       &(net_in.data[0]));
                exit_code = 7;
                sigint_handler(SIGINT);
                break;
            }
        }

    }
    return 0;
}


int main(int argc, char** argv)
{
    char readkey;
    bm_draw_logo(1);
    if ((argc != 3) && (argc != 2)) {
        printf("using: (<prog> port host_name) or (<prog> host_name)\n");
        return 0;
    }

    tcgetattr(0, &term_old_attributes);

    memset(net_msg.data, 0, sizeof(struct bm_net_msg));
    printf("Start new team or try to connect? [Y/N] >:");
    scanf("%c",&readkey);
    if (tolower(readkey) == 'y') {
        team_keeper = BM_TRUE;
        net_msg.data[0] = BM_NET_MSG_NEWGAME;
        printf("using: s - start game, q - exit.\n");
    } else {
        team_keeper = BM_FALSE;
        net_msg.data[0] = BM_NET_MSG_CONNECT;
        printf("using: arrows for moving, 1 - battle boom, 2 - set mine, ");
        printf("3 - use medicine\n_________\nenter nickname >:");
        scanf("%s", &(net_msg.data[1]));
    }

    if (argc == 3) {
        net_port = atoi(argv[1]);
        net_host_name = argv[2];
    } else {
        net_port = BM_DEFAULT_PORT;
        net_host_name = argv[1];
    }

    net_socket = socket(PF_INET, SOCK_STREAM, 0);
    net_host = gethostbyname(net_host_name);

    net_server_address.sin_family = AF_INET;
    net_server_address.sin_port = htons((short)net_port);
    /* if h_addr_list is empty - [0] value equal NULL */
    memcpy(&(net_server_address.sin_addr.s_addr),
            net_host->h_addr_list[0], net_host->h_length);

    if (connect(net_socket, (struct sockaddr*)(&net_server_address),
            sizeof(struct sockaddr_in)) == -1)
    {
        printf("connect fail\n");
        close(net_socket);
        return 1;
    }

    /** net initialization */
    net_send_msg();
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        printf("signal(SIGINT... failed\n");
        exit_code = 2;
        sigint_handler(SIGINT);
    }
    if (signal(SIGQUIT, sigint_handler) == SIG_ERR) {
        printf("signal(SIGQUIT... failed\n");
        exit_code = 2;
        sigint_handler(SIGINT);
    }
    if (team_keeper) {
        run_terminal_team_keeper();
    } else {
        run_terminal_client();
    }

    sigint_handler(SIGINT);
    return 0;
}
