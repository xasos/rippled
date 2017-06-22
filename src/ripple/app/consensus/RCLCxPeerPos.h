//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

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

#ifndef RIPPLE_APP_CONSENSUS_RCLCXPEERPOS_H_INCLUDED
#define RIPPLE_APP_CONSENSUS_RCLCXPEERPOS_H_INCLUDED

#include <ripple-libpp/src/ripple/src/basics/CountedObject.h>
#include <ripple/basics/base_uint.h>
#include <ripple/beast/hash/hash_append.h>
#include <ripple/consensus/ConsensusProposal.h>
#include <ripple-libpp/src/ripple/src/json/json_value.h>
#include <ripple-libpp/src/ripple/src/protocol/HashPrefix.h>
#include <ripple-libpp/src/ripple/src/protocol/PublicKey.h>
#include <ripple-libpp/src/ripple/src/protocol/SecretKey.h>
#include <chrono>
#include <cstdint>
#include <string>

namespace ripple {

/** A peer's signed, proposed position for use in RCLConsensus.

    Carries a ConsensusProposal signed by a peer.
*/
class RCLCxPeerPos : public CountedObject<RCLCxPeerPos>
{
public:
    static char const*
    getCountedObjectName()
    {
        return "RCLCxPeerPos";
    }
    using pointer = std::shared_ptr<RCLCxPeerPos>;
    using ref = const pointer&;

    //< The type of the proposed position
    using Proposal = ConsensusProposal<NodeID, uint256, uint256>;

    /** Constructor

        Constructs a signed peer position.

        @param publicKey Public key of the peer
        @param signature Signature provided with the proposal
        @param suppress ????
        @param proposal The consensus proposal
    */

    RCLCxPeerPos(
        PublicKey const& publicKey,
        Slice const& signature,
        uint256 const& suppress,
        Proposal&& proposal);

    //! Create the signing hash for the proposal
    uint256
    getSigningHash() const;

    //! Verify the signing hash of the proposal
    bool
    checkSign() const;

    //! Signature of the proposal (not necessarily verified)
    Slice
    getSignature() const
    {
        return signature_;
    }

    //! Public key of peer that sent the proposal
    PublicKey const&
    getPublicKey() const
    {
        return publicKey_;
    }

    //! ?????
    uint256 const&
    getSuppressionID() const
    {
        return mSuppression;
    }

    //! The consensus proposal
    Proposal const&
    proposal() const
    {
        return proposal_;
    }

    /// @cond Ignore
    //! Add a conversion operator to conform to the Consensus interface
    operator Proposal const&() const
    {
        return proposal_;
    }
    /// @endcond

    //! JSON representation of proposal
    Json::Value
    getJson() const;

private:
    template <class Hasher>
    void
    hash_append(Hasher& h) const
    {
        using beast::hash_append;
        hash_append(h, HashPrefix::proposal);
        hash_append(h, std::uint32_t(proposal().proposeSeq()));
        hash_append(h, proposal().closeTime());
        hash_append(h, proposal().prevLedger());
        hash_append(h, proposal().position());
    }

    Proposal proposal_;
    uint256 mSuppression;
    PublicKey publicKey_;
    Buffer signature_;
};

/** Calculate a unique identifier for a signed proposal.

    The identifier is based on all the fields that contribute to the signature,
    as well as the signature itself. The "last closed ledger" field may be
    omitted, but the signer will compute the signature as if this field was
    present. Recipients of the proposal will inject the last closed ledger in
    order to validate the signature. If the last closed ledger is left out, then
    it is considered as all zeroes for the purposes of signing.

    @param proposeHash The hash of the proposed position
    @param previousLedger The hash of the ledger the proposal is based upon
    @param proposeSeq Sequence number of the proposal
    @param closeTime Close time of the proposal
    @param publicKey Signer's public key
    @param signature Proposal signature
*/
uint256
proposalUniqueId(
    uint256 const& proposeHash,
    uint256 const& previousLedger,
    std::uint32_t proposeSeq,
    NetClock::time_point closeTime,
    Slice const& publicKey,
    Slice const& signature);

}  // ripple

#endif
