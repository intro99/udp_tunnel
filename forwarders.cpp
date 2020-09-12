#pragma once
#include "shared.cpp"
#include "transport_base.cpp"
#include "obfuscate_base.cpp"

int loop_transports_select(transport_base *local, transport_base *remote, obfuscate_base *obfuscator)
{
    struct pollfd fds[2];
    memset(fds, 0 , sizeof(fds));

    if (!local->started)
    {
        if (local->start() != EXIT_SUCCESS)
        {
            return EXIT_FAILURE;
        }
    }
    if (!remote->started)
    {
        if (remote->start() != EXIT_SUCCESS)
        {
            return EXIT_FAILURE;
        }
    }

    fds[0].fd = local->get_selectable();
    fds[0].events = POLLIN;
    fds[1].fd = remote->get_selectable();
    fds[1].events = POLLIN;

    int msglen, offset;
    char buffer[MTU_SIZE * 3];
    while (run)
    {
        msglen = poll(fds, 2, 3 * 60 * 1000);

        if (fds[0].revents == POLLIN)
        {
            msglen = local->receive(buffer + MTU_SIZE, &offset);

            if (msglen > 0)
            {
                if (obfuscator != nullptr)
                {
                    msglen = obfuscator->encipher(buffer + MTU_SIZE + offset, msglen);
                }

                if (msglen > 0)
                {
                    remote->send(buffer + MTU_SIZE + offset, msglen);
                }
            }
            else if (msglen < 0)
            {
                local->restart();
            }
        }

        if (fds[1].revents == POLLIN)
        {
            msglen = remote->receive(buffer + MTU_SIZE, &offset);

            if (msglen > 0)
            {
                if (obfuscator != nullptr)
                {
                    msglen = obfuscator->decipher(buffer + MTU_SIZE + offset, msglen);
                }

                if (msglen > 0)
                {
                    local->send(buffer + MTU_SIZE + offset, msglen);
                }
            }
            else if (msglen < 0)
            {
                remote->restart();
            }
        }
    }

    local->stop();
    remote->stop();

    return run ? EXIT_FAILURE : EXIT_SUCCESS;
}

int loop_transports_thread(transport_base *local, transport_base *remote, obfuscate_base *obfuscator)
{
    std::thread threads[2];

    if (!local->started)
    {
        if (local->start() != EXIT_SUCCESS)
        {
            return EXIT_FAILURE;
        }
    }
    if (!remote->started)
    {
        if (remote->start() != EXIT_SUCCESS)
        {
            return EXIT_FAILURE;
        }
    }

    threads[0] = std::thread([](transport_base *local, transport_base *remote, obfuscate_base *obfuscator)
    {
        int msglen, offset;
        char buffer[MTU_SIZE * 3];
        while (run)
        {
            msglen = local->receive(buffer + MTU_SIZE, &offset);

            if (msglen == 0)
            {
                continue;
            }
            else if (msglen < 0)
            {
                local->restart();
                continue;
            }

            if (obfuscator != nullptr)
            {
                msglen = obfuscator->encipher(buffer + MTU_SIZE + offset, msglen);

                if (msglen < 1)
                {
                    continue;
                }
            }

            remote->send(buffer + MTU_SIZE + offset, msglen);
        }
    }, std::cref(local), std::cref(remote), std::cref(obfuscator));

    threads[1] = std::thread([](transport_base *local, transport_base *remote, obfuscate_base *obfuscator)
    {
        int msglen, offset;
        char buffer[MTU_SIZE * 3];
        while (run)
        {
            msglen = remote->receive(buffer + MTU_SIZE, &offset);

            if (msglen == 0)
            {
                continue;
            }
            else if (msglen < 0)
            {
                remote->restart();
                continue;
            }

            if (obfuscator != nullptr)
            {
                msglen = obfuscator->decipher(buffer + MTU_SIZE + offset, msglen);

                if (msglen < 1)
                {
                    continue;
                }
            }

            local->send(buffer + MTU_SIZE + offset, msglen);
        }
    }, std::cref(local), std::cref(remote), std::cref(obfuscator));

    for (int i = 0; i < 2; i++)
    {
        threads[i].join();  
    }

    local->stop();
    remote->stop();

    return run ? EXIT_FAILURE : EXIT_SUCCESS;
}