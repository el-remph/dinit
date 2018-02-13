#include <cstdint>
#include <cstring>

// Client library for Dinit clients


using handle_t = uint32_t;
using cpbuffer_t = cpbuffer<1024>;

class cp_read_exception
{
    public:
    int errcode;
    cp_read_exception(int err) : errcode(err) { }
};

class cp_write_exception
{
    public:
    int errcode;
    cp_write_exception(int err) : errcode(err) { }
};

class cp_old_client_exception
{
    // no body
};

class cp_old_server_exception
{
    // no body
};


// Fill a circular buffer from a file descriptor, until it contains at least _rlength_ bytes.
// Throws cp_read_exception if the requested number of bytes cannot be read, with:
//     errcode = 0   if end of stream (remote end closed)
//     errcode = errno   if another error occurred
// Note that EINTR is ignored (i.e. the read will be re-tried).
inline void fill_buffer_to(cpbuffer_t &buf, int fd, int rlength)
{
    do {
        int r = buf.fill_to(fd, rlength);
        if (r == -1) {
            if (errno != EINTR) {
                throw cp_read_exception(errno);
            }
        }
        else if (r == 0) {
            throw cp_read_exception(0);
        }
        else {
            return;
        }
    }
    while (true);
}

// Wait for a reply packet, skipping over any information packets that are received in the meantime.
inline void wait_for_reply(cpbuffer_t &rbuffer, int fd)
{
    fill_buffer_to(rbuffer, fd, 1);

    while (rbuffer[0] >= 100) {
        // Information packet; discard.
        fill_buffer_to(rbuffer, fd, 2);
        int pktlen = (unsigned char) rbuffer[1];

        rbuffer.consume(1);  // Consume one byte so we'll read one byte of the next packet
        fill_buffer_to(rbuffer, fd, pktlen);
        rbuffer.consume(pktlen - 1);
    }
}

// Wait for an info packet. If any other reply packet comes, throw a cp_read_exception.
inline void wait_for_info(cpbuffer_t &rbuffer, int fd)
{
    fill_buffer_to(rbuffer, fd, 2);

    if (rbuffer[0] < 100) {
        throw cp_read_exception(0);
    }

    int pktlen = (unsigned char) rbuffer[1];
    fill_buffer_to(rbuffer, fd, pktlen);
}

// Write *all* the requested buffer and re-try if necessary until
// the buffer is written or an unrecoverable error occurs.
inline int write_all(int fd, const void *buf, size_t count)
{
    const char *cbuf = static_cast<const char *>(buf);
    int w = 0;
    while (count > 0) {
        int r = write(fd, cbuf, count);
        if (r == -1) {
            if (errno == EINTR) continue;
            return r;
        }
        w += r;
        cbuf += r;
        count -= r;
    }
    return w;
}

// Write all the requested buffer, and throw an exception on failure.
inline void write_all_x(int fd, const void *buf, size_t count)
{
    if (write_all(fd, buf, count) == -1) {
        throw cp_write_exception(errno);
    }
}

// Check the protocol version is compatible with the client.
//   minverison - minimum protocol version that client can speak
//   version - maximum protocol version that client can speak
//   rbuffer, fd -  communication buffer and socket
// returns: the actual protocol version
// throws an exception on protocol mismatch or error.
inline uint16_t check_protocol_version(int minversion, int version, cpbuffer_t &rbuffer, int fd)
{
    constexpr int bufsize = 1;
    char buf[bufsize] = { DINIT_CP_QUERYVERSION };
    write_all_x(fd, buf, bufsize);

    wait_for_reply(rbuffer, fd);
    if (rbuffer[0] != DINIT_RP_CPVERSION) {
        throw cp_read_exception{0};
    }

    // DINIT_RP_CVERSION, (2 byte) minimum compatible version, (2 byte) actual version
    constexpr int rbufsize = 1 + 2 * sizeof(uint16_t);
    fill_buffer_to(rbuffer, fd, rbufsize);
    uint16_t rminversion;
    uint16_t cpversion;

    rbuffer.extract(reinterpret_cast<char *>(&rminversion), 1, sizeof(uint16_t));
    rbuffer.extract(reinterpret_cast<char *>(&cpversion), 1 + sizeof(uint16_t), sizeof(uint16_t));
    rbuffer.consume(rbufsize);

    if (rminversion > version) {
        // We are too old
        throw cp_old_client_exception();
    }
    if (cpversion < minversion) {
        // Server is too old
        throw cp_old_server_exception();
    }

    return cpversion;
}
