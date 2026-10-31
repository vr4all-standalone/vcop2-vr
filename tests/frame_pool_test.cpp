// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/model2_capture.h"
#include <atomic>
#include <iostream>
#include <set>
#include <thread>
#include <vector>
#define CHECK(v) do{if(!(v)){std::cerr<<"Failed: " #v " at "<<__LINE__<<'\n';return 1;}}while(0)
using namespace vcop2;
static std::unique_ptr<NativeFrame> payload(uint64_t sequence,unsigned value) {
    auto frame=std::make_unique<NativeFrame>();frame->sequence=sequence;
    frame->background.assign(128,value);frame->foreground.assign(64,value+1);
    frame->native.assign(128,value+2);frame->sky.assign(256,value+3);
    frame->blobs.push_back({"texture0",std::vector<uint8_t>(4096,uint8_t(value))});
    return frame;
}
int main() {
    NativeFramePool pool;
    CHECK(pool.cached_count()==0);CHECK(!pool.publish({}));
    auto first=pool.publish(payload(1,7));auto reader=first;std::weak_ptr<const NativeFrame> old=reader;
    const auto* background=reader->background.data();const auto* bytes=reader->blobs[0].bytes.data();
    first.reset();CHECK(pool.cached_count()==0);
    auto unavailable=pool.acquire();CHECK(unavailable.background.empty());
    auto other=pool.publish(payload(2,9));
    CHECK(reader->sequence==1&&reader->background[0]==7&&reader->blobs[0].bytes[0]==7);
    CHECK(!old.expired());reader.reset();CHECK(old.expired());CHECK(pool.cached_count()==1);
    auto recycled=pool.acquire();CHECK(pool.cached_count()==0);
    CHECK(recycled.background.data()==background&&recycled.blobs[0].bytes.data()==bytes);
    auto current=pool.publish(std::make_unique<NativeFrame>(std::move(recycled)));
    CHECK(old.expired()&&!old.lock());CHECK(current->background.data()==background);
    CHECK(other->sequence==2&&other->background[0]==9);current.reset();other.reset();

    // Recycling a payload cannot resurrect its expired shared_ptr controlblock,
    // and readers on another thread can outlive arbitrary later publications.
    auto held=pool.publish(payload(100,23));std::atomic<bool> running{true},intact{true};std::atomic<unsigned> observations{};
    std::thread observer([held,&running,&intact,&observations]{
        while(running.load(std::memory_order_acquire)) {
            if(held->sequence!=100||held->blobs[0].bytes.front()!=23||held->blobs[0].bytes.back()!=23||held->native.back()!=25)
                intact.store(false,std::memory_order_relaxed);
            observations.fetch_add(1,std::memory_order_release);
        }
    });
    while(!observations.load(std::memory_order_acquire))std::this_thread::yield();
    for(unsigned n=0;n<200;n++) {
        auto storage=pool.acquire();SceneCapture capture;capture.begin(n+200,n,std::move(storage));
        std::array<uint8_t,8> pixels{};pixels.fill(uint8_t(n));capture.bytes("gamma",pixels.data(),pixels.size());
        auto next=pool.publish(std::make_unique<NativeFrame>(capture.take()));CHECK(next->blobs[0].bytes.front()==uint8_t(n));
    }
    running.store(false,std::memory_order_release);observer.join();CHECK(intact.load());
    CHECK(held->background.front()==23);held.reset();CHECK(pool.cached_count()<=NativeFramePool::MaxCached);

    // Final readers can retire on the render thread while the producer is
    // acquiring and publishing other frames through the same bounded cache.
    std::vector<std::shared_ptr<const NativeFrame>> batch;
    for(unsigned n=0;n<100;n++)batch.push_back(pool.publish(payload(1000+n,n)));
    std::atomic<bool> release_ready{};
    std::thread releaser([batch=std::move(batch),&release_ready]() mutable {
        release_ready.store(true,std::memory_order_release);
        for(auto& frame:batch){frame.reset();std::this_thread::yield();}
    });
    while(!release_ready.load(std::memory_order_acquire))std::this_thread::yield();
    for(unsigned n=0;n<200;n++){auto frame=pool.acquire();frame.sequence=n;auto published=pool.publish(std::make_unique<NativeFrame>(std::move(frame)));CHECK(published->sequence==n);}
    releaser.join();CHECK(pool.cached_count()<=NativeFramePool::MaxCached);

    NativeFramePool consumer_pool;auto consumer_frame=consumer_pool.publish(payload(1500,37));
    std::weak_ptr<const NativeFrame> consumer_weak=consumer_frame;
    std::atomic<unsigned> producer_steps{};std::atomic<bool> consumer_done{},consumer_intact{true};
    std::thread final_reader([reader=std::move(consumer_frame),&producer_steps,&consumer_done,&consumer_intact]() mutable {
        while(!producer_steps.load(std::memory_order_acquire))std::this_thread::yield();
        consumer_intact.store(reader->sequence==1500&&reader->blobs[0].bytes.front()==37&&reader->blobs[0].bytes.back()==37&&reader->sky.back()==40,std::memory_order_relaxed);
        reader.reset(); // The final strong release runs on this consumer.
        consumer_done.store(true,std::memory_order_release);
    });
    do {
        auto frame=consumer_pool.acquire();SceneCapture capture;capture.begin(1600,20,std::move(frame));
        std::array<uint8_t,4> gamma{1,2,3,4};capture.bytes("gamma",gamma.data(),gamma.size());
        auto next=consumer_pool.publish(std::make_unique<NativeFrame>(capture.take()));
        CHECK(next->blobs[0].bytes.back()==4);producer_steps.fetch_add(1,std::memory_order_release);
    }while(!consumer_done.load(std::memory_order_acquire));
    final_reader.join();CHECK(consumer_intact.load()&&consumer_weak.expired()&&!consumer_weak.lock());
    CHECK(consumer_pool.cached_count()<=NativeFramePool::MaxCached);

    NativeFramePool bounded;std::vector<std::shared_ptr<const NativeFrame>> readers;std::set<const uint32_t*> retained;
    for(unsigned n=0;n<6;n++){auto p=bounded.publish(payload(300+n,n));if(n<NativeFramePool::MaxCached)retained.insert(p->native.data());readers.push_back(std::move(p));}
    for(auto& p:readers)p.reset();
    CHECK(bounded.cached_count()==NativeFramePool::MaxCached);
    for(unsigned n=0;n<NativeFramePool::MaxCached;n++){auto frame=bounded.acquire();CHECK(retained.erase(frame.native.data())==1);}
    CHECK(bounded.cached_count()==0&&retained.empty());CHECK(bounded.acquire().native.empty());

    std::shared_ptr<const NativeFrame> survivor;std::weak_ptr<const NativeFrame> weak_survivor;
    {NativeFramePool temporary;survivor=temporary.publish(payload(500,31));weak_survivor=survivor;}
    CHECK(survivor->sequence==500&&survivor->blobs[0].bytes.back()==31);
    survivor.reset();CHECK(weak_survivor.expired());weak_survivor.reset();
    std::cout<<"Immutable retired snapshots, weak expiry, bounded retention and buffer reuse passed\n";
}
