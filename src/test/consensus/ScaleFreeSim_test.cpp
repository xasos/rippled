//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012-2016 Ripple Labs Inc->

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================
#include <BeastConfig.h>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <ripple/beast/clock/manual_clock.h>
#include <ripple/beast/unit_test.h>
#include <test/csf.h>
#include <test/csf/random.h>
#include <utility>

namespace ripple {
namespace test {

namespace csf
{
/** Randomly specify trust based on ranking of peers

    Generate a random trust graph based on a provided ranking of peers.

        1. Generates  `numUNL` random UNLs by sampling without replacement
            from the ranked nodes.
        2. Restricts the size of the random UNLs according to SizePDF
        3. Each node randomly selects one such generated UNL to use as its own.

    @param peers The group of peers to create trust over
    @param numUNLs The number of UNLs to create
    @param unlSizePDF Generates random integeres between (0,size-1) to
                        restrict the size of generated PDF
    @param Generator The uniform random bit generator to use

*/
template <class SizePDF, class Generator>
void
randomRankedTrust(
    PeerGroup & peers,
    std::vector<double> const & ranks,
    int numUNLs,
    SizePDF unlSizePDF,
    Generator& g)
{
    std::size_t const size = peers.size();
    assert(size == ranks.size());

    // 2. Generate UNLs based on sampling without replacement according
    //    to weights.
    std::vector<PeerGroup> unls(numUNLs);
    std::vector<Peer*> rawPeers(peers.begin(), peers.end());
    std::generate(unls.begin(), unls.end(), [&]() {
        std::vector<Peer*> res =
            random_weighted_shuffle(rawPeers, ranks, g);
        res.resize(unlSizePDF(g));
        return PeerGroup(std::move(res));
    });

    // 3. Assign trust
    std::uniform_int_distribution<int> u(0, numUNLs - 1);
    for(auto & peer : peers)
    {
        for(auto & target : unls[u(g)])
            peer->trust(*target);
    }
}

}

class ScaleFreeSim_test : public beast::unit_test::suite
{
    void
    run() override
    {
        using namespace std::chrono;
        using namespace csf;
        // Generate a quasi-random scale free network and simulate consensus
        // as we vary transaction submission rates

        // NOTE:
        // Currently This simulation is a work in progress and is being used to
        // explore the best design for the simulation framework

        int N = 100;  // Peers

        int numUNLs = 15;  //  UNL lists
        int minUNLSize = N / 4, maxUNLSize = N / 2;

        ConsensusParms parms;
        Sim sim;
        PeerGroup network = sim.createGroup(N);

        // generate trust ranks
        std::vector<double> ranks =
            sample(network.size(), PowerLawDistribution{1, 3}, sim.rng);

        // generate scale-free trust graph
        randomRankedTrust(network, ranks, numUNLs,
            std::uniform_int_distribution<>{minUNLSize, maxUNLSize},
            sim.rng);

        // nodes with a trust line in either direction are network-connected
        boost::mt19937 gen;
        boost::random::uniform_int_distribution<> dist(200, 400);
        //network.connectFromTrust(
        //    round<milliseconds>(0.001 * dist(gen) * parms.ledgerGRANULARITY));
        network.connectFromTrustRandom();

        //fixed delays, and randomly assign some a fraction of the connections to use one delay versus the other.

        // Initialize collectors to track statistics to report
        TxCollector txCollector;
        LedgerCollector ledgerCollector;
        auto colls = collectors(txCollector, ledgerCollector);
        sim.collectors.add(colls);

        // Initial round to set prior state
        sim.run(1);

        // Initialize timers
        HeartbeatTimer heart(sim.scheduler, seconds(10s));

        // Run for 10 minues, submitting 100 tx/second
        std::chrono::nanoseconds simDuration = 5min;
        std::chrono::nanoseconds quiet = 10s;
        Rate rate{100, 1000ms};

        // txs, start/stop/step, target
        auto peerSelector = selector(network.begin(),
                                     network.end(),
                                     ranks,
                                     sim.rng);
        auto txSubmitter = submitter(ConstantDistribution{rate.inv()},
                          sim.scheduler.now() + quiet,
                          sim.scheduler.now() + (simDuration - quiet),
                          peerSelector,
                          sim.scheduler,
                          sim.rng);

        // run simulation for given duration
        heart.start();
        sim.run(simDuration);

        BEAST_EXPECT(sim.forks() == 1);
        BEAST_EXPECT(sim.synchronized());

        // TODO: Clean up this formatting mess!!

        log << "Peers: " << network.size() << std::endl;
        log << "Simulated Duration: "
            << duration_cast<milliseconds>(simDuration).count()
            << " ms" << std::endl;
        log << "Forks: " << sim.forks() << std::endl;
        log << "Synchronized: " << (sim.synchronized() ? "Y" : "N")
            << std::endl;
        log << std::endl;

        txCollector.report(simDuration, log);
        ledgerCollector.report(simDuration, log);
        // Print summary?
        // # forks?  # of LCLs?
        // # peers
        // # tx submitted
        // # ledgers/sec etc.?
    }
};

BEAST_DEFINE_TESTSUITE_MANUAL(ScaleFreeSim, consensus, ripple);

}  // namespace test
}  // namespace ripple
