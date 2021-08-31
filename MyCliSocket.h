//
// Created by root on 2021/7/31.
//

#ifndef TMP_TEST_MYCLISOCKET_H
#define TMP_TEST_MYCLISOCKET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory>
#include <list>

/*
 * TODO: `getsockopt` and `setsockopt` how to implement. quite difficult
 * TODO: `read` and `write` how to implement. easy
 */

class MyCliSocket {
public:
    enum SOCKSTATE {
        SOCKCREATED = 1,
        SOCKCONNECTED = 2,
    };

public:
    mutable int sockFd;
    mutable int flags;

public:
    MyCliSocket() : flags(0), sockFd(-1) {
        prepareSocket();
    }
    ~MyCliSocket() {
        if (isCreated()) {
            closeSock();
        }
    }
    MyCliSocket(const MyCliSocket &) = delete;
    MyCliSocket & operator=(const MyCliSocket &) = delete;
    int getFd() const {
        return sockFd;
    }
    bool isCreated() const {
        return getFlag(SOCKCREATED);
    }
    bool isConnected() const {
        return getFlag(SOCKCONNECTED);
    }
    int connectWith(const struct sockaddr *servAddr, socklen_t servAddrLen, int nsec = 3,
            const sockaddr *cliAddr = nullptr, socklen_t cliAddrLen = 0) const {
        int rtVal;
        fd_set rset, wset;
        timeval tval{};
        int fMode;

        if (not isCreated()) {
            rtVal = prepareSocket();
            if (rtVal != 0) {
                return rtVal;
            }
        }
        if (isConnected()) {
            closeSock();
            rtVal = prepareSocket();
            if (rtVal != 0) {
                return rtVal;
            }
        }

        if (cliAddr != nullptr) {
            rtVal = bind(sockFd, cliAddr, cliAddrLen);
            if (rtVal != 0) {
                rtVal = errno;
                goto connectErr;
            }
        }

        fMode = fcntl(sockFd, F_GETFL, 0);
        fcntl(sockFd, F_SETFL, fMode | O_NONBLOCK);
        rtVal = connect(sockFd, servAddr, servAddrLen);
        if (rtVal == 0) {
            goto connectOk;
        }

        if (rtVal < 0 and errno != EINPROGRESS) {
            rtVal = errno;
            goto connectErr;
        }


        FD_ZERO(&rset);
        FD_SET(sockFd, &rset);
        wset = rset;
        tval.tv_sec = nsec;
        tval.tv_usec = 0;
        rtVal = select(sockFd + 1, &rset, &wset, nullptr, nsec ? &tval : nullptr);
        if (rtVal == 0) {
            rtVal = ETIMEDOUT;
            goto connectErr;
        }
        if (rtVal == -1) {
            rtVal = errno;
            goto connectErr;
        }
        if (FD_ISSET(sockFd, &rset) || FD_ISSET(sockFd, &wset)) {
            socklen_t len = sizeof(rtVal);
            if (getsockopt(sockFd, SOL_SOCKET, SO_ERROR, &rtVal, &len) < 0) {
                goto connectErr;
            }
            goto connectOk;
        }

        connectOk:
        fcntl(sockFd, F_SETFL, fMode);
        setFlag(SOCKCONNECTED);
        return 0;

        connectErr:
        closeSock();
        prepareSocket();
        return rtVal;
    }
    int connectWith(const struct sockaddr *servAddr, socklen_t servAddrLen,
            const sockaddr *cliAddr, socklen_t cliAddrLen, int nsec = 3) const {
        return connectWith(servAddr, servAddrLen, nsec, cliAddr, cliAddrLen);
    }
    /*
     * before use this two function, you should check whether sock is created.
     *
     * it's should note that i don't check if created in the function. cause it's
     * difficult to deal with not created.
     */
    int getSockOpt(int level, int optname, void *optval, socklen_t *optlen) const {
        return getsockopt(sockFd, level, optname, optval, optlen);
    }
    int setSockOpt(int level, int optname, const void *optval, socklen_t optlen) const {
        return setsockopt(sockFd, level, optname, optval, optlen);
    }

    int readSock(void *buf, size_t count) const {
        return read(sockFd, buf, count);
    }
    int writeSock(const void *buf, size_t count) const {
        return write(sockFd, buf, count);
    }

    int shutdownSock(int how = SHUT_WR) const {
        return shutdown(sockFd, how);
    }
    /*
     * could use after the call of `connectWith` and before the call `resetSock`.
     * but it's no effect.
     */
    int prepare(int times = 5) const {
        if (not isCreated()) {
            // `Connected` is a situation of `Created`
            return prepareSocket();
        }

        return 0;
    }
    int resetSock() const {
        closeSock();
        return prepareSocket();
    }

private:
    void setFlag(SOCKSTATE s) const {
        flags |= s;
    }
    void removeFlag(SOCKSTATE s) const {
        flags &= ~s;
    }
    bool getFlag(SOCKSTATE s) const {
        return flags & s;
    }
    int prepareSocket(int times = 5) const {
        int rtVal = 0;
        for (int i = 0; i < times; ++i) {
            sockFd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockFd != -1) {
                setFlag(SOCKCREATED);
                return 0;
            }

            rtVal = errno;
        }

        removeFlag(SOCKCREATED);
        return rtVal;
    }
    /*
     * create a socket for times.
     * return 0 if ok else return errno.
     * will set `SOCKNOTCREATED` or `SOCKCREATED`.
     */

    int closeSock() const {
        close(sockFd);
        removeFlag(SOCKCREATED);
        removeFlag(SOCKCONNECTED);
        sockFd = -1;
        return 0;
    }
};


#endif //TMP_TEST_MYCLISOCKET_H
