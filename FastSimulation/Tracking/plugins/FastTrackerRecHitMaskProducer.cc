// system includes                                                                                                                                
#include <memory>

// framework stuff
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Framework/interface/ESHandle.h"

// data formats
#include "DataFormats/Common/interface/ValueMap.h"
#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/TrackReco/interface/TrackFwd.h"
#include "DataFormats/TrackerRecHit2D/interface/FastTrackerRecHit.h"
#include "DataFormats/TrackerRecHit2D/interface/FastTrackerRecHitCollection.h"

// other
#include "TrackingTools/PatternTools/interface/TrackCollectionTokens.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "CommonTools/UtilAlgos/interface/TFileService.h"
#include "TH1.h"
#include "TH2.h"
#include <TH1D.h>
#include <TH2D.h>


class FastTrackerRecHitMaskProducer : public edm::stream::EDProducer <>
{
    public:

    explicit FastTrackerRecHitMaskProducer(const edm::ParameterSet&);

    ~FastTrackerRecHitMaskProducer() override {}

    void produce(edm::Event&, const edm::EventSetup&) override;


    private:

    // an alias
    using QualityMaskCollection = std::vector<unsigned char>;

    // tokens
    const int minNumberOfLayersWithMeasBeforeFiltering_;
    const reco::TrackBase::TrackQuality trackQuality_;

    const TrackCollectionTokens trajectories_;
    edm::EDGetTokenT<QualityMaskCollection> srcQuals;

    edm::EDGetTokenT<FastTrackerRecHitCollection>  recHits_;

    edm::EDGetTokenT<std::vector<bool> > oldHitMaskToken_;
  
  TH1D *recotracks,*track_p,*track_pt,*track_phi,*track_eta,*track_valid,*track_lost;

};

FastTrackerRecHitMaskProducer::FastTrackerRecHitMaskProducer(const edm::ParameterSet& iConfig) 
    : minNumberOfLayersWithMeasBeforeFiltering_(iConfig.getParameter<int>("minNumberOfLayersWithMeasBeforeFiltering"))
    , trackQuality_(reco::TrackBase::qualityByName(iConfig.getParameter<std::string>("TrackQuality")))
    , trajectories_(iConfig.getParameter<edm::InputTag>("trajectories"),consumesCollector())
    , recHits_(consumes<FastTrackerRecHitCollection>(iConfig.getParameter<edm::InputTag>("recHits")))
      

{
  edm::Service<TFileService> fs;
  recotracks = fs->make<TH1D>("tracks_size","Tracks size",100,0,100);
  track_p = fs->make<TH1D>("trackp","Track momentum",1000,0,1000);
  track_pt = fs->make<TH1D>("trackpt","Track pt",1000,0,1000);
  track_phi = fs->make<TH1D>("trackphi","Track phi",50,-5,5);
  track_eta = fs->make<TH1D>("trackseta","Track eta",50,-5,5);
  track_valid = fs->make<TH1D>("trackvalidhits","Tracks valid hits",100,0,100);
  track_lost = fs->make<TH1D>("tracklosthits","Track lost hits",100,0,100);
  
  produces<std::vector<bool> >();
  
  auto const & classifier = iConfig.getParameter<edm::InputTag>("trackClassifier");
  if ( !classifier.label().empty())
	srcQuals = consumes<QualityMaskCollection>(classifier);
  
  auto const &  oldHitRemovalInfo = iConfig.getParameter<edm::InputTag>("oldHitRemovalInfo");
  if (!oldHitRemovalInfo.label().empty()) {
    oldHitMaskToken_ = consumes<std::vector<bool> >(oldHitRemovalInfo);
  }
  
  
  
}

void FastTrackerRecHitMaskProducer::produce(edm::Event& iEvent, const edm::EventSetup& es)
{
    // the product

    std::unique_ptr<std::vector<bool> > collectedHits(new std::vector<bool>());

    // input

    edm::Handle<FastTrackerRecHitCollection> recHits;
    iEvent.getByToken(recHits_,recHits);


    if(!oldHitMaskToken_.isUninitialized()){
	edm::Handle<std::vector<bool> > oldHitMasks;
	iEvent.getByToken(oldHitMaskToken_,oldHitMasks);
	collectedHits->insert(collectedHits->begin(),oldHitMasks->begin(),oldHitMasks->end());
    }
    collectedHits->resize(recHits->size(),false);

    auto const & tracks = trajectories_.tracks(iEvent);

    QualityMaskCollection const * pquals=nullptr;
    if (!srcQuals.isUninitialized()) {
	edm::Handle<QualityMaskCollection> hqual;
	iEvent.getByToken(srcQuals, hqual);
	pquals = hqual.product();
    }

    // required quality
    unsigned char qualMask = ~0;
    if (trackQuality_!=reco::TrackBase::undefQuality) qualMask = 1<<trackQuality_; 
    std::cout<<"This code is running"<<std::endl;
    // loop over tracks and mask hits of selected tracks
    std::cout<<"Mask: Tracks size="<<tracks.size()<<std::endl;
    recotracks->Fill(tracks.size());
    for (auto i=0U; i<tracks.size(); ++i){
      	std::cout<<"Loop over tracks are running"<<std::endl;
        const reco::Track & track = tracks[i];
	std::cout<<"Outer momentum="<<track.outerMomentum()<<std::endl;
	track_p->Fill(track.outerP());
	track_pt->Fill(track.outerPt());
	track_phi->Fill(track.outerPhi());
	track_eta->Fill(track.outerEta());
	track_valid->Fill(track.found());
	track_lost->Fill(track.lost());
	bool goodTk =  (pquals) ? (*pquals)[i] & qualMask : track.quality(trackQuality_);
	if ( !goodTk) continue;
	if(track.hitPattern().trackerLayersWithMeasurement() < minNumberOfLayersWithMeasBeforeFiltering_) continue;
	for (auto hitIt = track.recHitsBegin() ;  hitIt != track.recHitsEnd(); ++hitIt) {
	    if(!(*hitIt)->isValid())
		continue;
	    const FastTrackerRecHit & hit = static_cast<const FastTrackerRecHit &>(*(*hitIt));
	    // note: for matched hits nIds() returns 2, otherwise 1
	    for(unsigned id_index = 0;id_index < hit.nIds();id_index++){
		(*collectedHits)[unsigned(hit.id(id_index))] = true;
	    }
	}
	
    }

    iEvent.put(std::move(collectedHits));
    
}

DEFINE_FWK_MODULE(FastTrackerRecHitMaskProducer);
