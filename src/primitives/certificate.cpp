#include "primitives/certificate.h"
#include "primitives/block.h"
#include "undo.h"
#include "coins.h"
#include "validationinterface.h"
#include "coins.h"
#include "core_io.h"
#include "miner.h"
#include "utilmoneystr.h"
#include "consensus/validation.h"
#include "sc/sidechain.h"
#include "txmempool.h"
#include "zen/forkmanager.h"

#include "main.h"

CScCertificate::CScCertificate() : CTransactionBase(),
    scId(), epochNumber(EPOCH_NULL), endEpochBlockHash(), totalAmount(), fee(), nonce() { }

CScCertificate::CScCertificate(const CMutableScCertificate &cert) :
    scId(cert.scId), epochNumber(cert.epochNumber), endEpochBlockHash(cert.endEpochBlockHash),
    totalAmount(cert.totalAmount), fee(cert.fee), nonce(cert.nonce)
{
    *const_cast<int*>(&nVersion) = cert.nVersion;
    *const_cast<std::vector<CTxOut>*>(&vout) = cert.vout;
    UpdateHash();
}

CScCertificate& CScCertificate::operator=(const CScCertificate &cert) {
    CTransactionBase::operator=(cert);
    //---
    *const_cast<uint256*>(&scId) = cert.scId;
    *const_cast<int32_t*>(&epochNumber) = cert.epochNumber;
    *const_cast<uint256*>(&endEpochBlockHash) = cert.endEpochBlockHash;
    *const_cast<CAmount*>(&totalAmount) = cert.totalAmount;
    *const_cast<CAmount*>(&fee) = cert.fee;
    *const_cast<uint256*>(&nonce) = cert.nonce;
    return *this;
}

CScCertificate::CScCertificate(const CScCertificate &cert) : epochNumber(0), totalAmount(0), fee(0) {
    // call explicitly the copy of members of virtual base class
    *const_cast<uint256*>(&hash) = cert.hash;
    *const_cast<int32_t*>(&nVersion) = cert.nVersion;
    *const_cast<std::vector<CTxOut>*>(&vout) = cert.vout;
    //---
    *const_cast<uint256*>(&scId) = cert.scId;
    *const_cast<int32_t*>(&epochNumber) = cert.epochNumber;
    *const_cast<uint256*>(&endEpochBlockHash) = cert.endEpochBlockHash;
    *const_cast<CAmount*>(&totalAmount) = cert.totalAmount;
    *const_cast<CAmount*>(&fee) = cert.fee;
    *const_cast<uint256*>(&nonce) = cert.nonce;
}

void CScCertificate::UpdateHash() const
{
    *const_cast<uint256*>(&hash) = SerializeHash(*this);
}

bool CScCertificate::CheckVersionBasic(CValidationState &state) const
{
    return true;
}

bool CScCertificate::CheckInputsAvailability(CValidationState &state) const
{
    //Currently there are no inputs for certificates
    if (!GetVin().empty())
    {
        return state.DoS(10, error("vin not empty"), REJECT_INVALID, "bad-cert-invalid");
    }

    return true;
}

bool CScCertificate::CheckOutputsAvailability(CValidationState &state) const
{
    // we allow empty certificate, but if we have no bt in vout the total amount must be 0
    if ((GetNumbOfBackwardTransfers() == 0) && totalAmount != 0) 
    {
        return state.DoS(10, error("vout empty and totalAmount != 0"), REJECT_INVALID, "bad-cert-invalid");
    }

    return true;
}

bool CScCertificate::CheckSerializedSize(CValidationState &state) const
{
    BOOST_STATIC_ASSERT(MAX_BLOCK_SIZE > MAX_CERT_SIZE); // sanity
    if (CalculateSize() > MAX_CERT_SIZE)
    {
        return state.DoS(100, error("size limits failed"), REJECT_INVALID, "bad-cert-oversize");
    }

    return true;
}

bool CScCertificate::CheckFeeAmount(const CAmount& totalVinAmount, CValidationState& state) const {
    return true;
}

CAmount CScCertificate::GetFeeAmount(CAmount /* unused */) const
{
    // this is a signed uint64, the caller must check if that is legal
    //    return (fee);
    // TODO cert: return 0 until MC owned fee will be handled
    return CAmount(0);
}

unsigned int CScCertificate::CalculateSize() const
{
    unsigned int sz = ::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION);
//    LogPrint("cert", "%s():%d -sz=%u\n", __func__, __LINE__, sz);
    return sz;
}

unsigned int CScCertificate::CalculateModifiedSize(unsigned int /* unused nTxSize*/) const
{
    return CalculateSize();
}

std::string CScCertificate::EncodeHex() const
{
    return EncodeHexCert(*this);
}

std::string CScCertificate::ToString() const
{
    std::string str;
    str += strprintf("CScCertificate(hash=%s, ver=%d, vout.size=%u, totAmount=%d.%08d, fee=%d.%08d\n)",
        GetHash().ToString().substr(0,10),
        nVersion,
        vout.size(),
        totalAmount / COIN, totalAmount % COIN,
        fee / COIN, fee % COIN);
    for (unsigned int i = 0; i < vout.size(); i++)
        str += "    " + vout[i].ToString() + "\n";

    return str;
}

void CScCertificate::AddToBlock(CBlock* pblock) const
{
    LogPrint("cert", "%s():%d - adding to block cert %s\n", __func__, __LINE__, GetHash().ToString());
    pblock->vcert.push_back(*this);
}

void CScCertificate::AddToBlockTemplate(CBlockTemplate* pblocktemplate, CAmount fee, unsigned int sigops) const
{
    LogPrint("cert", "%s():%d - adding to block templ cert %s, fee=%s, sigops=%u\n", __func__, __LINE__,
        GetHash().ToString(), FormatMoney(fee), sigops);
    pblocktemplate->vCertFees.push_back(fee);
    pblocktemplate->vCertSigOps.push_back(sigops);
}

bool CScCertificate::ContextualCheck(CValidationState& state, int nHeight, int dosLevel) const 
{
    bool areScSupported = zen::ForkManager::getInstance().areSidechainsSupported(nHeight);
    if (!areScSupported)
         return state.DoS(dosLevel, error("Sidechain are not supported"), REJECT_INVALID, "bad-cert-version");
    return true;
}

bool CScCertificate::CheckFinal(int flags) const
{
    // as of now certificate finality has yet to be defined (see tx.nLockTime)
    return true;
}


double CScCertificate::GetPriority(const CCoinsViewCache &view, int nHeight) const
{
    // TODO cert: for the time being return max prio, as shielded txes do
    return MAXIMUM_PRIORITY;
}

//--------------------------------------------------------------------------------------------
// binaries other than zend that are produced in the build, do not call these members and therefore do not
// need linking all of the related symbols. We use this macro as it is already defined with a similar purpose
// in zen-tx binary build configuration
#ifdef BITCOIN_TX

bool CScCertificate::TryPushToMempool(bool fLimitFree, bool fRejectAbsurdFee) {return true;}

bool CScCertificate::IsApplicableToState(CValidationState& state, int nHeight) const { return true; }
bool CScCertificate::IsStandard(std::string& reason, int nHeight) const { return true; }
#else
bool CScCertificate::TryPushToMempool(bool fLimitFree, bool fRejectAbsurdFee)
{
    CValidationState state;
    return ::AcceptCertificateToMemoryPool(mempool, state, *this, fLimitFree, nullptr, fRejectAbsurdFee);
}

bool CScCertificate::IsApplicableToState(CValidationState& state, int nHeight) const
{
    LogPrint("cert", "%s():%d - cert [%s]\n", __func__, __LINE__, GetHash().ToString());
    CCoinsViewCache view(pcoinsTip);
    return view.IsCertApplicableToState(*this, nHeight, state);
}
    
bool CScCertificate::IsStandard(std::string& reason, int nHeight) const
{
    if (!zen::ForkManager::getInstance().areSidechainsSupported(nHeight))
    {
        reason = "version";
        return false;
    }

    // so far just one version is supported, but in future it might not be the case
    if (zen::ForkManager::getInstance().getCertificateVersion(nHeight) != nVersion)
    {
        reason = "version";
        return false;
    }

    return CheckOutputsAreStandard(nHeight, reason);
}
#endif

void CScCertificate::addToScCommitment(std::map<uint256, uint256>& map, std::set<uint256>& sScIds) const
{
    sScIds.insert(scId);
    map[scId] = GetHash();
}

CAmount CScCertificate::GetValueOfBackwardTransfers() const
{
    CAmount nValueOut = 0;
    for (auto out : vout)
        if (out.isFromBackwardTransfer)
            nValueOut += out.nValue;
    return nValueOut;
}

int CScCertificate::GetNumbOfBackwardTransfers() const
{
    int size = 0;
    for (auto out : vout)
        if (out.isFromBackwardTransfer)
            size += 1;
    return size;
}


// Mutable Certificate
//-------------------------------------
CMutableScCertificate::CMutableScCertificate() :
        scId(), epochNumber(CScCertificate::EPOCH_NULL), endEpochBlockHash(), totalAmount(), fee(), nonce() {}

CMutableScCertificate::CMutableScCertificate(const CScCertificate& cert) :
    scId(cert.GetScId()), epochNumber(cert.epochNumber), endEpochBlockHash(cert.endEpochBlockHash),
    totalAmount(cert.totalAmount), fee(cert.fee), nonce(cert.nonce)
{
    nVersion = cert.nVersion;
    vout = cert.GetVout();
}

uint256 CMutableScCertificate::GetHash() const
{
    return SerializeHash(*this);
}


