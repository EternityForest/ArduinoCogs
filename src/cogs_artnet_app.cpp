#include "cogs_rules.h"
#include "cogs_artnet.h"
#include "cogs_global_events.h"
#include <WiFiUdp.h>

namespace cogs_artnet
{
    bool started = false;

    unsigned char last = 0;
    unsigned long lastFrame = 0;

    // the UDP port to listen on
    const uint16_t listenPort = 6454;
    uint8_t outputBuffer[513];
    int universeNumber = 0;

    std::shared_ptr<cogs_rules::IntTagPoint> artnetFrame = nullptr;

    WiFiUDP udp;

    float readArtnet(float v)
    {
        int p = v;
        p += 1;
        if (p > 512)
        {
            return 0;
        }
        return outputBuffer[p];
    }

    void slowPoll()
    {
        // if (!udp)
        // {
        //     udp.begin(listenPort);
        //     udp.joinMulticastGroup(IPAddress(239, 255, 255, 250));
        // }
    }

    void fastPoll()
    {
        int ps = udp.parsePacket();
        if (ps > 0)
        {
            unsigned char buf[ps];
            udp.read(buf, ps);
            if (memcmp(buf, "Art-Net", 7) == 0)
            {
                if (((uint16_t *)(buf + 8))[0] == 0x5000)
                {
                    int universe = ((uint16_t *)(buf + 14))[0];

                    int seq = buf[12];
                    if (seq - last > 192){
                        if (millis() - lastFrame < 250){
                            last = seq;
                            return;
                        }
                    }

                    lastFrame = millis();
                    last = seq;

                    int p = 18;
                    int op = 0;
                    while ((p < ps) && (op < 512))
                    {
                        outputBuffer[op++] = buf[p++];
                    }
                    artnetFrame->bang();
                }
            }
        }
    }

    void begin()
    {
        if (!started)
        {
            artnetFrame = cogs_rules::IntTagPoint::getTag("artnet.frame", 0);

            udp.begin(listenPort);
            udp.beginMulticast(IPAddress(239, 255, 255, 250), listenPort);
            started = true;
            cogs::slowPollHandlers.push_back(slowPoll);
            cogs::fastPollHandlers.push_back(fastPoll);

            cogs_rules::addUserFunction1("artnet", readArtnet);
            cogs_rules::refreshBindingsEngine();
        }
    }
}