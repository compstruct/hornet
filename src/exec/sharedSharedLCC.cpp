// -*- mode:c++; c-style:k&r; c-basic-offset:5; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "sharedSharedLCC.hpp"
#include "messages.hpp"
#include <boost/function.hpp>
#include <boost/bind.hpp>

#define PRINT_PROGRESS
//#undef PRINT_PROGRESS

#define DEADLOCK_CHECK
#undef DEADLOCK_CHECK

#define DEBUG
//#undef DEBUG

#ifdef DEBUG
#define mh_log(X) if(true) cout
#define mh_assert(X) assert(X)
#else
#define mh_assert(X) 
#define mh_log(X) LOG(log,X)
#endif

/* Used for tentative stats per memory instruction (outstanding costs survive) */
#define T_IDX_CAT 0
#define T_IDX_L1 1
#define T_IDX_L2 2

#define EXTEND_AND_CUT_MIN_DELTA 8 
#define EXTEND_AND_CUT_DEFAULT_WRITE_DELTA 32 

static uint32_t network_distance(uint32_t a, uint32_t b, uint32_t w) {
    return ((a%w > b%w)? a%w - b%w : b%w - a%w) + ((a/w > b/w)? a/w - b/w : b/w - a/w);
}

/***********************************************************/
/* Cache helper functions to customize the cache behaviors */
/***********************************************************/

/* copiers */

static shared_ptr<void> copy_coherence_info_ideal(shared_ptr<void> source) { 
    shared_ptr<sharedSharedLCC::coherenceInfo> ret
        (new sharedSharedLCC::coherenceInfo(*static_pointer_cast<sharedSharedLCC::coherenceInfo>(source)));
    /* pointer of timestamp is passed */
    return ret;
}

static shared_ptr<void> copy_coherence_info_nonideal(shared_ptr<void> source) { 
    shared_ptr<sharedSharedLCC::coherenceInfo> ret
        (new sharedSharedLCC::coherenceInfo(*static_pointer_cast<sharedSharedLCC::coherenceInfo>(source)));
    /* copy of timestamp value is passed */
    ret->timestamp = shared_ptr<uint64_t>(new uint64_t(*ret->timestamp));
    return ret;
}

/* replacement policies */

static uint32_t replacement_away_expired_home_evictable_lru_ideal(vector<uint32_t>& evictables, 
                                                                     cacheLine const* lines,
                                                                     const uint64_t& system_time,
                                                                     shared_ptr<random_gen> ran) 
{
    vector<uint32_t> away_expired;
    vector<uint32_t> home_evictable;
    vector<uint32_t> other;

    for (vector<uint32_t>::iterator i = evictables.begin(); i != evictables.end(); ++i) {
        shared_ptr<sharedSharedLCC::coherenceInfo> info = 
            static_pointer_cast<sharedSharedLCC::coherenceInfo>(lines[*i].coherence_info);
        if (info->away && !info->checked_out && system_time < *info->timestamp) {
            away_expired.push_back(*i);
            home_evictable.clear();
            other.clear();
        } else if (away_expired.empty() && !info->away && !info->checked_out) {
            home_evictable.push_back(*i);
            other.clear();
        } else if (away_expired.empty() && home_evictable.empty()) {
            other.push_back(*i);
        }
    }

    uint64_t min = UINT64_MAX;
    uint32_t tgt = 0;
    vector<uint32_t>& candidates = (!away_expired.empty())? away_expired : (!home_evictable.empty())? home_evictable : other;
    for (vector<uint32_t>::iterator i = candidates.begin(); i != candidates.end(); ++i) {
        if (lines[*i].last_access_time < min) {
            tgt = *i;
            min = lines[*i].last_access_time;
        }
    }
    return tgt;
}

static uint32_t replacement_away_expired_home_evictable_random_ideal(vector<uint32_t>& evictables, 
                                                                        cacheLine const* lines,
                                                                        const uint64_t& system_time,
                                                                        shared_ptr<random_gen> ran) 
{
    vector<uint32_t> away_expired;
    vector<uint32_t> home_evictable;
    vector<uint32_t> other;

    for (vector<uint32_t>::iterator i = evictables.begin(); i != evictables.end(); ++i) {
        shared_ptr<sharedSharedLCC::coherenceInfo> info = 
            static_pointer_cast<sharedSharedLCC::coherenceInfo>(lines[*i].coherence_info);
        if (info->away && !info->checked_out && system_time < *info->timestamp) {
            away_expired.push_back(*i);
            home_evictable.clear();
            other.clear();
        } else if (away_expired.empty() && !info->away && !info->checked_out) {
            home_evictable.push_back(*i);
            other.clear();
        } else if (away_expired.empty() && home_evictable.empty()) {
            other.push_back(*i);
        }
    }

    vector<uint32_t>& candidates = (!away_expired.empty())? away_expired : (!home_evictable.empty())? home_evictable : other;
    return candidates[ran->random_range(candidates.size())];
}

static uint32_t replacement_away_expired_home_evictable_lru_nonideal_L1(vector<uint32_t>& evictables, 
                                                                        cacheLine const* lines,
                                                                        const uint64_t& system_time,
                                                                        shared_ptr<random_gen> ran) 
{
    vector<uint32_t> away_expired;
    vector<uint32_t> home_evictable;
    vector<uint32_t> other;

    for (vector<uint32_t>::iterator i = evictables.begin(); i != evictables.end(); ++i) {
        shared_ptr<sharedSharedLCC::coherenceInfo> info = 
            static_pointer_cast<sharedSharedLCC::coherenceInfo>(lines[*i].coherence_info);
        if (info->away && !info->checked_out && system_time < *info->timestamp) {
            away_expired.push_back(*i);
            home_evictable.clear();
            other.clear();
        } else if (away_expired.empty() && !info->away) {
            home_evictable.push_back(*i);
            other.clear();
        } else if (away_expired.empty() && home_evictable.empty()) {
            other.push_back(*i);
        }
    }

    uint64_t min = UINT64_MAX;
    uint32_t tgt = 0;
    vector<uint32_t>& candidates = (!away_expired.empty())? away_expired : (!home_evictable.empty())? home_evictable : other;
    for (vector<uint32_t>::iterator i = candidates.begin(); i != candidates.end(); ++i) {
        if (lines[*i].last_access_time < min) {
            tgt = *i;
            min = lines[*i].last_access_time;
        }
    }
    return tgt;
}

static uint32_t replacement_away_expired_home_evictable_random_nonideal_L1(vector<uint32_t>& evictables, 
                                                                           cacheLine const* lines,
                                                                           const uint64_t& system_time,
                                                                           shared_ptr<random_gen> ran) 
{
    vector<uint32_t> away_expired;
    vector<uint32_t> home_evictable;
    vector<uint32_t> other;

    for (vector<uint32_t>::iterator i = evictables.begin(); i != evictables.end(); ++i) {
        shared_ptr<sharedSharedLCC::coherenceInfo> info = 
            static_pointer_cast<sharedSharedLCC::coherenceInfo>(lines[*i].coherence_info);
        if (info->away && !info->checked_out && system_time < *info->timestamp) {
            away_expired.push_back(*i);
            home_evictable.clear();
            other.clear();
        } else if (away_expired.empty() && !info->away) {
            home_evictable.push_back(*i);
            other.clear();
        } else if (away_expired.empty() && home_evictable.empty()) {
            other.push_back(*i);
        }
    }

    vector<uint32_t>& candidates = (!away_expired.empty())? away_expired : (!home_evictable.empty())? home_evictable : other;
    return candidates[ran->random_range(candidates.size())];
}


static uint32_t replacement_away_expired_home_evictable_lru_nonideal_L2(vector<uint32_t>& evictables, 
                                                                        cacheLine const* lines,
                                                                        const uint64_t& system_time,
                                                                        shared_ptr<random_gen> ran) 
{
    vector<uint32_t> home_evictable;
    vector<uint32_t> other;

    for (vector<uint32_t>::iterator i = evictables.begin(); i != evictables.end(); ++i) {
        shared_ptr<sharedSharedLCC::coherenceInfo> info = 
            static_pointer_cast<sharedSharedLCC::coherenceInfo>(lines[*i].coherence_info);
        if (!info->checked_out && system_time >= *info->timestamp) {
            home_evictable.push_back(*i);
            other.clear();
        } else if (home_evictable.empty()) {
            other.push_back(*i);
        }
    }

    uint64_t min = UINT64_MAX;
    uint32_t tgt = 0;
    vector<uint32_t>& candidates = (!home_evictable.empty())? home_evictable : other;
    for (vector<uint32_t>::iterator i = candidates.begin(); i != candidates.end(); ++i) {
        if (lines[*i].last_access_time < min) {
            tgt = *i;
            min = lines[*i].last_access_time;
        }
    }
    return tgt;
}

static uint32_t replacement_away_expired_home_evictable_random_nonideal_L2(vector<uint32_t>& evictables, 
                                                                           cacheLine const* lines,
                                                                           const uint64_t& system_time,
                                                                           shared_ptr<random_gen> ran) 
{
    vector<uint32_t> home_evictable;
    vector<uint32_t> other;

    for (vector<uint32_t>::iterator i = evictables.begin(); i != evictables.end(); ++i) {
        shared_ptr<sharedSharedLCC::coherenceInfo> info = 
            static_pointer_cast<sharedSharedLCC::coherenceInfo>(lines[*i].coherence_info);
        if (!info->checked_out && system_time >= *info->timestamp) {
            home_evictable.push_back(*i);
            other.clear();
        } else if (home_evictable.empty()) {
            other.push_back(*i);
        }
    }

    vector<uint32_t>& candidates = (!home_evictable.empty())? home_evictable : other;
    return candidates[ran->random_range(candidates.size())];
}

/* hit checkers */

static bool hit_checker_ideal_LCC(shared_ptr<cacheRequest> req, cacheLine& line, const uint64_t& system_time) { 
    shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info = 
        static_pointer_cast<sharedSharedLCC::coherenceInfo>(line.coherence_info);
    shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info = 
        static_pointer_cast<sharedSharedLCC::auxInfoForCoherence>(req->aux_info_for_coherence());

    if (req->request_type() == CACHE_REQ_UPDATE || req->request_type() == CACHE_REQ_INVALIDATE) {
        return true;
    }

    /* read or write */
    if (request_info->is_read) {
        if (system_time > *coherence_info->timestamp) {
            if (coherence_info->away) {
                return false;
            } else if (request_info->current_core != request_info->issued_core) {
                line.coherence_info_dirty = true;
                if (request_info->cfg.max_timestamp_delta_for_read_copy == 0) {
                    coherence_info->timestamp = shared_ptr<uint64_t>(new uint64_t(UINT64_MAX));
                } else {
                    coherence_info->timestamp = 
                        shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_read_copy));
                }
            }
        }
    } else if (coherence_info->away) {
        return false;
    } else if (*(coherence_info->timestamp) > system_time) {
        /* writing at home core, timestamp unexpired -> do the magic!  */
        *(coherence_info->timestamp) = system_time;
    }
    return true;
}

static bool hit_checker_ideal_WLCC(shared_ptr<cacheRequest> req, cacheLine& line, const uint64_t& system_time) { 
    shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info = 
        static_pointer_cast<sharedSharedLCC::coherenceInfo>(line.coherence_info);
    shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info = 
        static_pointer_cast<sharedSharedLCC::auxInfoForCoherence>(req->aux_info_for_coherence());

    if (req->request_type() == CACHE_REQ_UPDATE || req->request_type() == CACHE_REQ_INVALIDATE) {
        return true;
    }

    /* read or write */
    if (coherence_info->checked_out) {
        if (coherence_info->away) {
            return true;
        } else {
            *(coherence_info->timestamp) = system_time;
            return false;
        }
    }

    /* non checked-out line */
    if (request_info->is_read) {
        if (system_time > *coherence_info->timestamp) {
            if (coherence_info->away) {
                return false;
            } else if (request_info->current_core != request_info->issued_core) {
                line.coherence_info_dirty = true;
                if (request_info->cfg.max_timestamp_delta_for_read_copy == 0) {
                    coherence_info->timestamp = shared_ptr<uint64_t>(new uint64_t(UINT64_MAX));
                } else {
                    coherence_info->timestamp = 
                        shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_read_copy));
                }
            }
        }
    } else if (coherence_info->away) {
        return false;
    } else {
        /* writing at home core */
        if (*(coherence_info->timestamp) > system_time) {
            *(coherence_info->timestamp) = system_time;
        }
        if (request_info->current_core != request_info->issued_core) {
            coherence_info->checked_out = true;
            line.coherence_info_dirty = true;
            if (request_info->cfg.max_timestamp_delta_for_write_copy == 0) {
                coherence_info->timestamp = shared_ptr<uint64_t>(new uint64_t(UINT64_MAX));
            } else {
                coherence_info->timestamp = 
                    shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_write_copy));
            }
        }
    }
    return true;
}

static bool hit_checker_fixed_LCC(shared_ptr<cacheRequest> req, cacheLine& line, const uint64_t& system_time) { 
    shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info = 
        static_pointer_cast<sharedSharedLCC::coherenceInfo>(line.coherence_info);
    shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info = 
        static_pointer_cast<sharedSharedLCC::auxInfoForCoherence>(req->aux_info_for_coherence());

    if (req->request_type() == CACHE_REQ_UPDATE || req->request_type() == CACHE_REQ_INVALIDATE) {
        return true;
    }

    /* read or write */
    if (request_info->is_read) {
        if (system_time > *coherence_info->timestamp) {
            if (coherence_info->away) {
                return false;
            } else if (request_info->current_core != request_info->issued_core) {
                coherence_info->timestamp = 
                    shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_read_copy));
                line.coherence_info_dirty = true;
            }
        }
    } else if (coherence_info->away) {
        return false;
    } else if (system_time <= *coherence_info->timestamp) {
        return false;
    }
    return true;
}

static bool hit_checker_fixed_WLCC(shared_ptr<cacheRequest> req, cacheLine& line, const uint64_t& system_time) { 
    shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info = 
        static_pointer_cast<sharedSharedLCC::coherenceInfo>(line.coherence_info);
    shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info = 
        static_pointer_cast<sharedSharedLCC::auxInfoForCoherence>(req->aux_info_for_coherence());

    if (req->request_type() == CACHE_REQ_UPDATE || req->request_type() == CACHE_REQ_INVALIDATE) {
        return true;
    }

    /* read or write */

    if (coherence_info->checked_out) {
        if (coherence_info->away) {
            return true;
        } else {
            return false;
        }
    }

    if (request_info->is_read) {
        if (system_time > *coherence_info->timestamp) {
            if (coherence_info->away) {
                return false;
            } else if (request_info->current_core != request_info->issued_core) {
                coherence_info->timestamp = 
                    shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_read_copy));
                line.coherence_info_dirty = true;
            }
        }
    } else if (coherence_info->away) {
        return false;
    } else if (system_time <= *coherence_info->timestamp) {
        return false;
    } else if (request_info->current_core != request_info->issued_core) {
        coherence_info->checked_out = true;
        line.coherence_info_dirty = true;
        coherence_info->timestamp = 
            shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_write_copy));
    }
    return true;
}

static bool hit_checker_period_prediction_LCC(shared_ptr<cacheRequest> req, cacheLine& line, const uint64_t& system_time) { 
    shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info = 
        static_pointer_cast<sharedSharedLCC::coherenceInfo>(line.coherence_info);
    shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info = 
        static_pointer_cast<sharedSharedLCC::auxInfoForCoherence>(req->aux_info_for_coherence());

    if (req->request_type() == CACHE_REQ_UPDATE || req->request_type() == CACHE_REQ_INVALIDATE) {
        return true;
    }

    /* read or write */

    /* update period prediction */
    if (!request_info->is_read) {
        if (coherence_info->last_write_time != 0) {
            coherence_info->last_write_period = system_time - coherence_info->last_write_time;
        }
        coherence_info->last_write_time = system_time;
        line.coherence_info_dirty = true;
    } else if (request_info->current_core != request_info->issued_core) {
        if (coherence_info->last_remote_read_time != 0) {
            coherence_info->last_remote_read_time = system_time - coherence_info->last_remote_read_time;
        }
        coherence_info->last_remote_read_time = system_time;
        line.coherence_info_dirty = true;
    }

    if (request_info->is_read) {
        if (system_time > *coherence_info->timestamp) {
            if (coherence_info->away) {
                return false;
            } else if (request_info->current_core != request_info->issued_core) {
                /* calculate delta */
                uint64_t delta = 0;
                if (coherence_info->last_remote_read_period && coherence_info->last_write_period) {
                    uint32_t dist = network_distance(request_info->current_core, request_info->issued_core, 
                                                     request_info->cfg.network_width);
                    delta = 2 * dist * coherence_info->last_write_period / coherence_info->last_remote_read_period;
                    if (delta > request_info->cfg.max_timestamp_delta_for_read_copy) {
                        delta = request_info->cfg.max_timestamp_delta_for_read_copy;
                    }
                }
                line.coherence_info_dirty = true;
                coherence_info->timestamp = shared_ptr<uint64_t>(new uint64_t(system_time + delta));
            }
        }
    } else if (coherence_info->away) {
        return false;
    } else if (system_time <= *coherence_info->timestamp) {
        return false;
    }
    return true;
}

static bool hit_checker_period_prediction_WLCC(shared_ptr<cacheRequest> req, cacheLine& line, const uint64_t& system_time) { 
    shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info = 
        static_pointer_cast<sharedSharedLCC::coherenceInfo>(line.coherence_info);
    shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info = 
        static_pointer_cast<sharedSharedLCC::auxInfoForCoherence>(req->aux_info_for_coherence());

    if (req->request_type() == CACHE_REQ_UPDATE || req->request_type() == CACHE_REQ_INVALIDATE) {
        return true;
    }

    /* read or write */

    /* update period prediction */
    if (!request_info->is_read) {
        if (coherence_info->last_write_time != 0) {
            coherence_info->last_write_period = system_time - coherence_info->last_write_time;
        }
        coherence_info->last_write_time = system_time;
        line.coherence_info_dirty = true;
    } else if (request_info->current_core != request_info->issued_core) {
        if (coherence_info->last_remote_read_time != 0) {
            coherence_info->last_remote_read_time = system_time - coherence_info->last_remote_read_time;
        }
        coherence_info->last_remote_read_time = system_time;
        line.coherence_info_dirty = true;
    }

    if (coherence_info->checked_out) {
        if (coherence_info->away) {
            return true;
        } else {
            return false;
        }
    }

    if (request_info->is_read) {
        if (system_time > *coherence_info->timestamp) {
            if (coherence_info->away) {
                return false;
            } else if (request_info->current_core != request_info->issued_core) {
                /* calculate delta */
                uint64_t delta = 0;
                if (coherence_info->last_remote_read_period && coherence_info->last_write_period) {
                    uint32_t dist = network_distance(request_info->current_core, request_info->issued_core, 
                                                     request_info->cfg.network_width);
                    delta = 2 * dist * coherence_info->last_write_period / coherence_info->last_remote_read_period;
                    if (delta > request_info->cfg.max_timestamp_delta_for_read_copy) {
                        delta = request_info->cfg.max_timestamp_delta_for_read_copy;
                    }
                }
                line.coherence_info_dirty = true;
                coherence_info->timestamp = shared_ptr<uint64_t>(new uint64_t(system_time + delta));
            }
        }
    } else if (coherence_info->away) {
        return false;
    } else if (system_time <= *coherence_info->timestamp) {
        return false;
    } else if (request_info->current_core != request_info->issued_core) {

        /* calculate delta */
        uint64_t delta = 0;
        if (coherence_info->last_local_access_period && coherence_info->last_write_period) {
            uint32_t dist = network_distance(request_info->current_core, request_info->issued_core, 
                                             request_info->cfg.network_width);
            delta = 2 * dist * coherence_info->last_local_access_period / coherence_info->last_write_period;
            if (delta > request_info->cfg.max_timestamp_delta_for_read_copy) {
                delta = request_info->cfg.max_timestamp_delta_for_read_copy;
            }
        }
        if (delta > 0) {
            coherence_info->checked_out = true;
            line.coherence_info_dirty = true;
        }
        coherence_info->timestamp = shared_ptr<uint64_t>(new uint64_t(system_time + delta));
    }
    return true;
}

static bool hit_checker_extend_and_cut_LCC(shared_ptr<cacheRequest> req, cacheLine& line, const uint64_t& system_time) { 
    shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info = 
        static_pointer_cast<sharedSharedLCC::coherenceInfo>(line.coherence_info);
    shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info = 
        static_pointer_cast<sharedSharedLCC::auxInfoForCoherence>(req->aux_info_for_coherence());

    if (req->request_type() == CACHE_REQ_UPDATE || req->request_type() == CACHE_REQ_INVALIDATE) {
        return true;
    }

    /* update shared bit if home core */
    if (!coherence_info->away && coherence_info->first_copyholder != request_info->issued_core) {
        coherence_info->shared = true;
    } 

    if (request_info->is_read) {
        /* read */
        if (coherence_info->away) {
            /* away data */
            if (system_time > *coherence_info->timestamp) {
                return false;
            }
        } else {
            /* at the library */
            if (coherence_info->shared && request_info->expired_amount > 0) {
                /* if not shared, always the maximum delta */
                if (coherence_info->current_delta * 2 <= request_info->cfg.max_timestamp_delta_for_read_copy ||
                    coherence_info->current_delta * 2 <= request_info->cfg.max_timestamp_delta_for_write_copy)
                {
                    coherence_info->current_delta *= 2;
                }
            }
            if (system_time > *coherence_info->timestamp || request_info->expired_amount > 0) {
                if (request_info->current_core != request_info->issued_core) {
                    line.coherence_info_dirty = true;
                    coherence_info->timestamp = (coherence_info->current_delta < request_info->cfg.max_timestamp_delta_for_read_copy) ?
                        shared_ptr<uint64_t>(new uint64_t(system_time + coherence_info->current_delta)) :
                        shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_read_copy));
                }
            }
        }
    } else if (coherence_info->away) {
        /* away write (always miss) */
        return false;
    } else if (system_time <= *coherence_info->timestamp) {
        /* write at library, ts not expired */
        if (coherence_info->current_delta > EXTEND_AND_CUT_MIN_DELTA) {
            coherence_info->current_delta /= 2;
        }
        return false;
    }
    return true;
}

static bool hit_checker_extend_and_cut_WLCC(shared_ptr<cacheRequest> req, cacheLine& line, const uint64_t& system_time) { 
    shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info = 
        static_pointer_cast<sharedSharedLCC::coherenceInfo>(line.coherence_info);
    shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info = 
        static_pointer_cast<sharedSharedLCC::auxInfoForCoherence>(req->aux_info_for_coherence());

    if (req->request_type() == CACHE_REQ_UPDATE || req->request_type() == CACHE_REQ_INVALIDATE) {
        return true;
    }

    /* update shared bit if home core */
    if (!coherence_info->away && coherence_info->first_copyholder != request_info->issued_core) {
        coherence_info->shared = true;
    } 

    if (coherence_info->checked_out) {
        if (coherence_info->away) {
            return true;
        } else {
            if (coherence_info->current_delta > EXTEND_AND_CUT_MIN_DELTA) {
                coherence_info->current_delta /= 2;
            }
            return false;
        }
    }

    if (request_info->is_read) {
        /* read */
        if (coherence_info->away) {
            /* away data */
            if (system_time > *coherence_info->timestamp) {
                return false;
            }
        } else {
            /* at the library */
            if (coherence_info->shared && request_info->expired_amount > 0) {
                /* if not shared, always the maximum delta */
                if (coherence_info->current_delta * 2 <= request_info->cfg.max_timestamp_delta_for_read_copy ||
                    coherence_info->current_delta * 2 <= request_info->cfg.max_timestamp_delta_for_write_copy)
                {
                    coherence_info->current_delta *= 2;
                }
            }
            if (system_time > *coherence_info->timestamp || request_info->expired_amount > 0) {
                if (request_info->current_core != request_info->issued_core) {
                    line.coherence_info_dirty = true;
                    coherence_info->timestamp = shared_ptr<uint64_t>(new uint64_t(system_time + coherence_info->current_delta));
                    if (!coherence_info->shared) {
                        coherence_info->checked_out = true;
                        coherence_info->timestamp = (coherence_info->current_delta < request_info->cfg.max_timestamp_delta_for_write_copy) ?
                            shared_ptr<uint64_t>(new uint64_t(system_time + coherence_info->current_delta)) :
                            shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_write_copy));
                    } else {
                        coherence_info->timestamp = (coherence_info->current_delta < request_info->cfg.max_timestamp_delta_for_read_copy) ?
                            shared_ptr<uint64_t>(new uint64_t(system_time + coherence_info->current_delta)) :
                            shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_read_copy));
                    }
                }
            }
        }
    } else {
        /* write */
        if (coherence_info->away) {
            /* away write (if not checked out, miss) */
            return false;
        } else if (system_time <= *coherence_info->timestamp) {
            /* write at library, ts not expired */
            if (!coherence_info->shared && request_info->issued_core == coherence_info->first_copyholder) {
                if (coherence_info->current_delta * 2 <= request_info->cfg.max_timestamp_delta_for_write_copy) {
                    coherence_info->current_delta *= 2;
                }
            } else if (coherence_info->current_delta > EXTEND_AND_CUT_MIN_DELTA) {
                coherence_info->current_delta /= 2;
            }
            return false;
        } else if (request_info->current_core != request_info->issued_core) {
            coherence_info->checked_out = true;
            line.coherence_info_dirty = true;
            if (coherence_info->current_delta * 2 <= request_info->cfg.max_timestamp_delta_for_write_copy) {
                coherence_info->current_delta *= 2;
            }
            coherence_info->timestamp = (coherence_info->current_delta < request_info->cfg.max_timestamp_delta_for_write_copy) ?
                shared_ptr<uint64_t>(new uint64_t(system_time + coherence_info->current_delta)) :
                shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_write_copy));
        }
    }
    return true;

}

/* evictable checker */

static bool can_evict_line_L2_ideal(cacheLine &line, const uint64_t& system_time) {
    shared_ptr<sharedSharedLCC::coherenceInfo> info = 
        static_pointer_cast<sharedSharedLCC::coherenceInfo>(line.coherence_info);
    if (info->home) {
        if (info->checked_out) {
            *info->timestamp = system_time;
            return false;
        } else if (system_time < *info->timestamp) {
            *info->timestamp = system_time;
        }
    }
    return true;
}


static bool can_evict_line_L2_nonideal(cacheLine &line, const uint64_t& system_time) {
    shared_ptr<sharedSharedLCC::coherenceInfo> info = 
        static_pointer_cast<sharedSharedLCC::coherenceInfo>(line.coherence_info);
    if (info->home) {
        if (info->checked_out || system_time < *info->timestamp) {
            return false;
        }
    }
    return true;
}


/*******************************/
/* Work table helper functions */
/*******************************/

/* called before a remote rep is cached at a local L1 */

static bool handler_for_remote_rep(shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info, 
                                   shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info,
                                   const uint64_t& system_time)
{
    coherence_info->away = true;
    return true;
}

/* called before a new L2 line is created from a dramctrl reply */

static bool handler_for_dramctrl_rep_ideal_LCC(shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info, 
                                               shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info,
                                               const uint64_t& system_time)
{
    coherence_info->checked_out = false;
    coherence_info->away = false;;
    coherence_info->home = request_info->current_core;

    if (request_info->cfg.max_timestamp_delta_for_read_copy == 0) {
        coherence_info->timestamp = shared_ptr<uint64_t>(new uint64_t(UINT64_MAX));
    } else {
        coherence_info->timestamp = 
            shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_read_copy));
    }
    return true;
}

static bool handler_for_dramctrl_rep_ideal_WLCC(shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info, 
                                               shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info,
                                               const uint64_t& system_time)
{
    coherence_info->checked_out = false;
    coherence_info->away = false;;
    coherence_info->home = request_info->current_core;

    if (request_info->current_core != request_info->issued_core) {
        if (request_info->is_read) {
            if (request_info->cfg.max_timestamp_delta_for_read_copy == 0) {
                coherence_info->timestamp = shared_ptr<uint64_t>(new uint64_t(UINT64_MAX));
            } else {
                coherence_info->timestamp = 
                    shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_read_copy));
            }
        } else {
            coherence_info->checked_out = true;
            if (request_info->cfg.max_timestamp_delta_for_write_copy == 0) {
                coherence_info->timestamp = shared_ptr<uint64_t>(new uint64_t(UINT64_MAX));
            } else {
                coherence_info->timestamp = 
                    shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_write_copy));
            }
        }
    } else {
        coherence_info->timestamp = shared_ptr<uint64_t>(new uint64_t(system_time));
    }

    return true;
}

static bool handler_for_dramctrl_rep_fixed_LCC(shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info, 
                                               shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info,
                                               const uint64_t& system_time)
{
    coherence_info->checked_out = false;
    coherence_info->away = false;
    coherence_info->home = request_info->current_core;
    if (request_info->is_read && request_info->issued_core != request_info->current_core) {
        coherence_info->timestamp = 
            shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_read_copy));
    } else {
        coherence_info->timestamp = 
            shared_ptr<uint64_t>(new uint64_t(system_time));
    }
    return true;
}

static bool handler_for_dramctrl_rep_fixed_WLCC(shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info, 
                                               shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info,
                                               const uint64_t& system_time)
{
    coherence_info->checked_out = false;
    coherence_info->away = false;
    coherence_info->home = request_info->current_core;
    if (request_info->issued_core != request_info->current_core) {
        if (request_info->is_read) {
            coherence_info->timestamp = 
                shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_read_copy));
        } else {
            coherence_info->checked_out = true;
            coherence_info->timestamp = 
                shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_read_copy));
        }
    } else {
        coherence_info->timestamp = 
            shared_ptr<uint64_t>(new uint64_t(system_time));
    }
    return true;

}

static bool handler_for_dramctrl_rep_period_prediction_LCC(shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info, 
                                               shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info,
                                               const uint64_t& system_time)
{
    coherence_info->checked_out = false;
    coherence_info->away = false;
    coherence_info->home = request_info->current_core;
    coherence_info->timestamp = shared_ptr<uint64_t>(new uint64_t(system_time));
    coherence_info->last_remote_read_time = 0;
    coherence_info->last_remote_read_period = 0;
    coherence_info->last_write_time = 0;
    coherence_info->last_write_period = 0;

    return true;
}

static bool handler_for_dramctrl_rep_period_prediction_WLCC(shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info, 
                                               shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info,
                                               const uint64_t& system_time)
{
    coherence_info->checked_out = false;
    coherence_info->away = false;
    coherence_info->home = request_info->current_core;
    coherence_info->timestamp = shared_ptr<uint64_t>(new uint64_t(system_time));
    coherence_info->last_remote_read_time = 0;
    coherence_info->last_remote_read_period = 0;
    coherence_info->last_write_time = 0;
    coherence_info->last_write_period = 0;
    coherence_info->last_local_access_time = 0;
    coherence_info->last_local_access_period = 0;

    return true;
}

static bool handler_for_dramctrl_rep_extend_and_cut_LCC(shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info, 
                                               shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info,
                                               const uint64_t& system_time)
{
    coherence_info->checked_out = false;
    coherence_info->away = false;
    coherence_info->home = request_info->current_core;
    coherence_info->timestamp = shared_ptr<uint64_t>(new uint64_t(system_time));
    coherence_info->last_remote_read_time = 0;
    coherence_info->last_remote_read_period = 0;
    coherence_info->last_write_time = 0;
    coherence_info->last_write_period = 0;
    coherence_info->shared = false;
    coherence_info->first_copyholder =  request_info->issued_core;
    coherence_info->current_delta = 
        (request_info->is_read) ? EXTEND_AND_CUT_MIN_DELTA :
                                  EXTEND_AND_CUT_DEFAULT_WRITE_DELTA;
    return true;
}

static bool handler_for_dramctrl_rep_extend_and_cut_WLCC(shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info, 
                                               shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info,
                                               const uint64_t& system_time)
{
    coherence_info->checked_out = false;
    coherence_info->away = false;
    coherence_info->home = request_info->current_core;
    coherence_info->timestamp = shared_ptr<uint64_t>(new uint64_t(system_time));
    coherence_info->last_remote_read_time = 0;
    coherence_info->last_remote_read_period = 0;
    coherence_info->last_write_time = 0;
    coherence_info->last_write_period = 0;
    coherence_info->last_local_access_time = 0;
    coherence_info->last_local_access_period = 0;

    coherence_info->shared = false;
    coherence_info->first_copyholder = request_info->issued_core;
    coherence_info->current_delta = EXTEND_AND_CUT_DEFAULT_WRITE_DELTA;
    return true;
}


/* called before a write on a line finishes waiting for an unexpired timestamp */

static bool handler_for_waited_for_timestamp_LCC(shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info, 
                                               shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info,
                                               const uint64_t& system_time)
{
    /* no action necessary */
    return false;
}

static bool handler_for_waited_for_timestamp_fixed_WLCC(shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info, 
                                               shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info,
                                               const uint64_t& system_time)
{
    if (request_info->current_core != request_info->issued_core) {
        coherence_info->checked_out = true;
        coherence_info->timestamp = 
            shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_write_copy));
        return true;
    }
    return false;
}

static bool handler_for_waited_for_timestamp_period_prediction_WLCC(shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info, 
                                               shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info,
                                               const uint64_t& system_time)
{
    if (request_info->current_core != request_info->issued_core) {

        coherence_info->checked_out = true;
        
        /* calculate delta */
        uint64_t delta = 0;
        if (coherence_info->last_local_access_period && coherence_info->last_write_period) {
            uint32_t dist = network_distance(request_info->current_core, request_info->issued_core, 
                                             request_info->cfg.network_width);
            delta = 2 * dist * coherence_info->last_local_access_period / coherence_info->last_write_period;
            if (delta > request_info->cfg.max_timestamp_delta_for_read_copy) {
                delta = request_info->cfg.max_timestamp_delta_for_read_copy;
            }
        }
        coherence_info->timestamp = shared_ptr<uint64_t>(new uint64_t(system_time + delta));
        return true;
    }
    return false;
}

static bool handler_for_waited_for_timestamp_extend_and_cut_WLCC(shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info, 
                                               shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info,
                                               const uint64_t& system_time)
{
    if (request_info->current_core != request_info->issued_core) {

        coherence_info->checked_out = true;
        
        coherence_info->timestamp = 
            (coherence_info->current_delta <= request_info->cfg.max_timestamp_delta_for_write_copy) ?
            shared_ptr<uint64_t>(new uint64_t(system_time + coherence_info->current_delta)) :
            shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_write_copy));
        return true;
    }
    return false;
}

/* called before a request on a line finishes waiting for a remote check in */

static bool handler_for_remote_checkin_ideal_WLCC(shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info, 
                                               shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info,
                                               const uint64_t& system_time)
{
    coherence_info->away = false;;

    if (request_info && request_info->current_core != request_info->issued_core) {
        if (request_info->is_read) {
            coherence_info->checked_out = false;
            if (request_info->cfg.max_timestamp_delta_for_read_copy == 0) {
                coherence_info->timestamp = shared_ptr<uint64_t>(new uint64_t(UINT64_MAX));
            } else {
                coherence_info->timestamp = 
                    shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_read_copy));
            }
        } else {
            if (request_info->cfg.max_timestamp_delta_for_write_copy == 0) {
                coherence_info->timestamp = shared_ptr<uint64_t>(new uint64_t(UINT64_MAX));
            } else {
                coherence_info->timestamp = 
                    shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_write_copy));
            }
        }
    } else {
        coherence_info->checked_out = false;
    }

    return true;
}

static bool handler_for_remote_checkin_fixed_WLCC(shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info, 
                                               shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info,
                                               const uint64_t& system_time)
{
    coherence_info->away = false;;

    if (request_info && request_info->current_core != request_info->issued_core) {
        if (request_info->is_read) {
            coherence_info->checked_out = false;
            coherence_info->timestamp = 
                shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_read_copy));
        } else {
            coherence_info->checked_out = true;
            coherence_info->timestamp = 
                shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_write_copy));
        }
    } else {
        coherence_info->checked_out = false;
    }

    return true;

}

static bool handler_for_remote_checkin_period_prediction_WLCC(shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info, 
                                               shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info,
                                               const uint64_t& system_time)
{
    coherence_info->away = false;;

    if (request_info && request_info->current_core != request_info->issued_core) {
        if (request_info->is_read) {
            coherence_info->checked_out = false;

            /* calculate delta */
            uint64_t delta = 0;
            if (coherence_info->last_remote_read_period && coherence_info->last_write_period) {
                uint32_t dist = network_distance(request_info->current_core, request_info->issued_core, 
                                                 request_info->cfg.network_width);
                delta = 2 * dist * coherence_info->last_write_period / coherence_info->last_remote_read_period;
                if (delta > request_info->cfg.max_timestamp_delta_for_read_copy) {
                    delta = request_info->cfg.max_timestamp_delta_for_read_copy;
                }
            }
            coherence_info->timestamp = shared_ptr<uint64_t>(new uint64_t(system_time + delta));

        } else {

            coherence_info->checked_out = true;

            /* calculate delta */
            uint64_t delta = 0;
            if (coherence_info->last_local_access_period && coherence_info->last_write_period) {
                uint32_t dist = network_distance(request_info->current_core, request_info->issued_core, 
                                                 request_info->cfg.network_width);
                delta = 2 * dist * coherence_info->last_local_access_period / coherence_info->last_write_period;
                if (delta > request_info->cfg.max_timestamp_delta_for_read_copy) {
                    delta = request_info->cfg.max_timestamp_delta_for_read_copy;
                }
            }
            coherence_info->timestamp = shared_ptr<uint64_t>(new uint64_t(system_time + delta));

        }
    } else {
        coherence_info->checked_out = false;
    }

    return true;
}

static bool handler_for_remote_checkin_extend_and_cut_WLCC(shared_ptr<sharedSharedLCC::coherenceInfo> coherence_info, 
                                               shared_ptr<sharedSharedLCC::auxInfoForCoherence> request_info,
                                               const uint64_t& system_time)
{
    coherence_info->away = false;;

    if (request_info && request_info->current_core != request_info->issued_core) {
        if (request_info->is_read) {

            coherence_info->checked_out = false;

            coherence_info->timestamp = 
                (coherence_info->current_delta <= request_info->cfg.max_timestamp_delta_for_read_copy) ?
                shared_ptr<uint64_t>(new uint64_t(system_time + coherence_info->current_delta)) :
                shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_read_copy));

        } else {

            coherence_info->checked_out = true;

            coherence_info->timestamp = 
                (coherence_info->current_delta <= request_info->cfg.max_timestamp_delta_for_write_copy) ?
                shared_ptr<uint64_t>(new uint64_t(system_time + coherence_info->current_delta)) :
                shared_ptr<uint64_t>(new uint64_t(system_time + request_info->cfg.max_timestamp_delta_for_write_copy));

        }
    } else {
        coherence_info->checked_out = false;
    }

    return true;
}

/***************************/
/* Shared-L1 Shared-L2 LCC */
/***************************/

sharedSharedLCC::sharedSharedLCC(uint32_t id, 
                                   const uint64_t &t, 
                                   shared_ptr<tile_statistics> st, 
                                   logger &l, 
                                   shared_ptr<random_gen> r, 
                                   shared_ptr<cat> a_cat, 
                                   sharedSharedLCCCfg_t cfg) :
    memory(id, t, st, l, r), 
    m_cfg(cfg), 
    m_l1(NULL), 
    m_l2(NULL), 
    m_cat(a_cat), 
    m_stats(shared_ptr<sharedSharedLCCStatsPerTile>()),
    m_work_table_vacancy_shared(cfg.work_table_size_shared),
    m_work_table_vacancy_read_exclusive(cfg.work_table_size_read_exclusive),
    m_work_table_vacancy_send_checkin_exclusive(cfg.work_table_size_send_checkin_exclusive),
    m_work_table_vacancy_receive_checkin_exclusive(cfg.work_table_size_receive_checkin_exclusive),
    m_available_core_ports(cfg.num_local_core_ports),
    m_handler_for_dramctrl_rep(NULL),
    m_handler_for_waited_for_timestamp(NULL),
    m_handler_for_remote_rep(NULL),
    m_handler_for_remote_checkin(NULL)
{
    /* sanity checks */
    if (m_cfg.bytes_per_flit == 0) throw err_bad_shmem_cfg("flit size must be non-zero.");
    if (m_cfg.words_per_cache_line == 0) throw err_bad_shmem_cfg("cache line size must be non-zero.");
    if (m_cfg.lines_in_l1 == 0) throw err_bad_shmem_cfg("sharedSharedLCC : L1 size must be non-zero.");
    if (m_cfg.lines_in_l2 == 0) throw err_bad_shmem_cfg("sharedSharedLCC : L2 size must be non-zero.");
    if (m_cfg.work_table_size_shared == 0) 
        throw err_bad_shmem_cfg("sharedSharedLCC : shared work table size must be non-zero.");
    if (m_cfg.work_table_size_read_exclusive== 0) 
        throw err_bad_shmem_cfg("sharedSharedLCC : read-exclusive work table size must be non-zero.");
    if (m_cfg.work_table_size_send_checkin_exclusive == 0 && m_cfg.use_checkout_for_write_copy) 
        throw err_bad_shmem_cfg("sharedSharedLCC : send-checkin-exclusive work table size must be non-zero for WLCC.");
    if (m_cfg.work_table_size_receive_checkin_exclusive == 0 && m_cfg.use_checkout_for_write_copy) 
        throw err_bad_shmem_cfg("sharedSharedLCC : receive-checkin-exclusive work table size must be non-zero for WLCC.");
    if (m_cfg.migration_logic == MIGRATION_ALWAYS_ON_WRITES && m_cfg.use_checkout_for_write_copy)
        throw err_bad_shmem_cfg("sharedSharedLCC : EM/LCC hybrid cannot use checkout.");

    /* create caches */
    replacementPolicy_t l1_policy, l2_policy;

    switch (cfg.l1_replacement_policy) {
    case _REPLACE_LRU:
        l1_policy = REPLACE_LRU;
        break;
    case _REPLACE_RANDOM:
        l1_policy = REPLACE_RANDOM;
        break;
    default:
        l1_policy = REPLACE_CUSTOM;
        break;
    }

    switch (cfg.l2_replacement_policy) {
    case _REPLACE_LRU:
        l2_policy = REPLACE_LRU;
        break;
    case _REPLACE_RANDOM:
        l2_policy = REPLACE_RANDOM;
        break;
    default:
        l2_policy = REPLACE_CUSTOM;
        break;
    }

    m_l1 = new cache(1, id, t, st, l, r, 
                     cfg.words_per_cache_line, cfg.lines_in_l1, cfg.l1_associativity, l1_policy,
                     cfg.l1_hit_test_latency, cfg.l1_num_read_ports, cfg.l1_num_write_ports);
    m_l2 = new cache(2, id, t, st, l, r, 
                     cfg.words_per_cache_line, cfg.lines_in_l2, cfg.l2_associativity, l2_policy,
                     cfg.l2_hit_test_latency, cfg.l2_num_read_ports, cfg.l2_num_write_ports);

    /* helpers */
    m_handler_for_remote_rep = &handler_for_remote_rep;
    if (m_cfg.timestamp_logic == TIMESTAMP_IDEAL) {
        m_copy_coherence_info = &copy_coherence_info_ideal;
        m_l1->set_helper_copy_coherence_info(&copy_coherence_info_ideal);
        m_l2->set_helper_copy_coherence_info(&copy_coherence_info_ideal);
        m_l2->set_helper_can_evict_line(&can_evict_line_L2_ideal);

        if (!m_cfg.use_checkout_for_write_copy) {
            m_l1->set_helper_is_coherence_hit(&hit_checker_ideal_LCC);
            m_l2->set_helper_is_coherence_hit(&hit_checker_ideal_LCC);

            m_handler_for_dramctrl_rep = &handler_for_dramctrl_rep_ideal_LCC;
        } else {
            m_l1->set_helper_is_coherence_hit(&hit_checker_ideal_WLCC);
            m_l2->set_helper_is_coherence_hit(&hit_checker_ideal_WLCC);

            m_handler_for_dramctrl_rep = &handler_for_dramctrl_rep_ideal_WLCC;
            m_handler_for_remote_checkin = &handler_for_remote_checkin_ideal_WLCC;
        }

        if (m_cfg.l1_replacement_policy == _REPLACE_AWAY_EXPIRED_HOME_EVICTABLE_LRU) {
            m_l1->set_helper_replacement_policy(&replacement_away_expired_home_evictable_lru_ideal);
        } else if (m_cfg.l1_replacement_policy == _REPLACE_AWAY_EXPIRED_HOME_EVICTABLE_RANDOM) {
            m_l1->set_helper_replacement_policy(&replacement_away_expired_home_evictable_random_ideal);
        }
        if (m_cfg.l2_replacement_policy == _REPLACE_AWAY_EXPIRED_HOME_EVICTABLE_LRU) {
            m_l2->set_helper_replacement_policy(&replacement_away_expired_home_evictable_lru_ideal);
        } else if (m_cfg.l2_replacement_policy == _REPLACE_AWAY_EXPIRED_HOME_EVICTABLE_RANDOM) {
            m_l2->set_helper_replacement_policy(&replacement_away_expired_home_evictable_random_ideal);
        }

    } else {
        m_copy_coherence_info = &copy_coherence_info_nonideal;
        m_l1->set_helper_copy_coherence_info(&copy_coherence_info_nonideal);
        m_l2->set_helper_copy_coherence_info(&copy_coherence_info_nonideal);
        m_l2->set_helper_can_evict_line(&can_evict_line_L2_nonideal);

        if (m_cfg.timestamp_logic == TIMESTAMP_FIXED) {
            if (!m_cfg.use_checkout_for_write_copy) {
                m_l1->set_helper_is_coherence_hit(&hit_checker_fixed_LCC);
                m_l2->set_helper_is_coherence_hit(&hit_checker_fixed_LCC);

                m_handler_for_dramctrl_rep = &handler_for_dramctrl_rep_fixed_LCC;
                m_handler_for_waited_for_timestamp = &handler_for_waited_for_timestamp_LCC;
            } else {
                m_l1->set_helper_is_coherence_hit(&hit_checker_fixed_WLCC);
                m_l2->set_helper_is_coherence_hit(&hit_checker_fixed_WLCC);

                m_handler_for_dramctrl_rep = &handler_for_dramctrl_rep_fixed_WLCC;
                m_handler_for_waited_for_timestamp = &handler_for_waited_for_timestamp_fixed_WLCC;
                m_handler_for_remote_checkin = &handler_for_remote_checkin_fixed_WLCC;
            }
        } else if (m_cfg.timestamp_logic == TIMESTAMP_PERIOD_PREDICTION) {
            if (!m_cfg.use_checkout_for_write_copy) {
                m_l1->set_helper_is_coherence_hit(&hit_checker_period_prediction_LCC);
                m_l2->set_helper_is_coherence_hit(&hit_checker_period_prediction_LCC);

                m_handler_for_dramctrl_rep = &handler_for_dramctrl_rep_period_prediction_LCC;
                m_handler_for_waited_for_timestamp = &handler_for_waited_for_timestamp_LCC;
            } else {
                m_l1->set_helper_is_coherence_hit(&hit_checker_period_prediction_WLCC);
                m_l2->set_helper_is_coherence_hit(&hit_checker_period_prediction_WLCC);

                m_handler_for_dramctrl_rep = &handler_for_dramctrl_rep_period_prediction_WLCC;
                m_handler_for_waited_for_timestamp = &handler_for_waited_for_timestamp_period_prediction_WLCC;
                m_handler_for_remote_checkin = &handler_for_remote_checkin_period_prediction_WLCC;
            }
        } else if (m_cfg.timestamp_logic == TIMESTAMP_EXTEND_AND_CUT) {
            if (!m_cfg.use_checkout_for_write_copy) {
                m_l1->set_helper_is_coherence_hit(&hit_checker_extend_and_cut_LCC);
                m_l2->set_helper_is_coherence_hit(&hit_checker_extend_and_cut_LCC);

                m_handler_for_dramctrl_rep = &handler_for_dramctrl_rep_extend_and_cut_LCC;
                m_handler_for_waited_for_timestamp = &handler_for_waited_for_timestamp_LCC; 
            } else {
                m_l1->set_helper_is_coherence_hit(&hit_checker_extend_and_cut_WLCC);
                m_l2->set_helper_is_coherence_hit(&hit_checker_extend_and_cut_WLCC);

                m_handler_for_dramctrl_rep = &handler_for_dramctrl_rep_extend_and_cut_WLCC; 
                m_handler_for_waited_for_timestamp = &handler_for_waited_for_timestamp_extend_and_cut_WLCC;
                m_handler_for_remote_checkin = &handler_for_remote_checkin_extend_and_cut_WLCC; 
            }

        }

        if (m_cfg.l1_replacement_policy == _REPLACE_AWAY_EXPIRED_HOME_EVICTABLE_LRU) {
            m_l1->set_helper_replacement_policy(&replacement_away_expired_home_evictable_lru_nonideal_L1);
        } else if (m_cfg.l1_replacement_policy == _REPLACE_AWAY_EXPIRED_HOME_EVICTABLE_RANDOM) {
            m_l1->set_helper_replacement_policy(&replacement_away_expired_home_evictable_random_nonideal_L1);
        }
        if (m_cfg.l2_replacement_policy == _REPLACE_AWAY_EXPIRED_HOME_EVICTABLE_LRU) {
            m_l2->set_helper_replacement_policy(&replacement_away_expired_home_evictable_lru_nonideal_L2);
        } else if (m_cfg.l2_replacement_policy == _REPLACE_AWAY_EXPIRED_HOME_EVICTABLE_RANDOM) {
            m_l2->set_helper_replacement_policy(&replacement_away_expired_home_evictable_random_nonideal_L2);
        }
    }


    /* schedule queue memory reservation */
    uint32_t total_size = m_cfg.work_table_size_shared + m_cfg.work_table_size_read_exclusive 
        + m_cfg.work_table_size_send_checkin_exclusive + m_cfg.work_table_size_receive_checkin_exclusive;
    m_cat_req_schedule_q.reserve(total_size);
    m_l1_read_req_schedule_q.reserve(total_size);
    m_l1_write_req_schedule_q.reserve(total_size);
    m_l2_read_req_schedule_q.reserve(total_size);
    m_l2_write_req_schedule_q.reserve(total_size);
    m_remote_req_schedule_q.reserve(total_size);
    m_remote_rep_and_checkin_schedule_q.reserve(total_size);
    m_dramctrl_req_schedule_q.reserve(total_size + 1);
    m_dramctrl_rep_schedule_q.reserve(total_size);

}

sharedSharedLCC::~sharedSharedLCC() {
    delete m_l1;
    delete m_l2;
}

uint32_t sharedSharedLCC::number_of_mem_msg_types() { return NUM_MSG_TYPES; }

void sharedSharedLCC::request(shared_ptr<memoryRequest> req) {

    /* assumes a request is not across multiple cache lines */
    uint32_t __attribute__((unused)) byte_offset = req->maddr().address%(m_cfg.words_per_cache_line*4);
    mh_assert( (byte_offset + req->word_count()*4) <= m_cfg.words_per_cache_line * 4);

    /* set status to wait */
    set_req_status(req, REQ_WAIT);

    /* per memory instruction info */
    if (req->per_mem_instr_runtime_info()) {
        shared_ptr<shared_ptr<void> > p_runtime_info = req->per_mem_instr_runtime_info();
        shared_ptr<void>& runtime_info = *p_runtime_info;
        shared_ptr<sharedSharedLCCStatsPerMemInstr> per_mem_instr_stats;
        if (!runtime_info) {
            /* no per-instr stats: this is the first time this memory instruction is issued */
            per_mem_instr_stats = shared_ptr<sharedSharedLCCStatsPerMemInstr>(new sharedSharedLCCStatsPerMemInstr(req->is_read()));
            per_mem_instr_stats->set_serialization_begin_time_at_current_core(system_time);
            runtime_info = per_mem_instr_stats;
        } else {
            per_mem_instr_stats = 
                static_pointer_cast<sharedSharedLCCStatsPerMemInstr>(*req->per_mem_instr_runtime_info());
            if (per_mem_instr_stats->is_in_migration()) {
                per_mem_instr_stats = static_pointer_cast<sharedSharedLCCStatsPerMemInstr>(runtime_info);
                per_mem_instr_stats->migration_finished(system_time, stats_enabled());
                per_mem_instr_stats->set_serialization_begin_time_at_current_core(system_time);
            }
        }
    }

    /* will schedule for a core port and a work table entry in schedule function */
    m_core_port_schedule_q.push_back(req);
}

void sharedSharedLCC::tick_positive_edge() {
    /* schedule and make requests */
#ifdef PRINT_PROGRESS
    static uint64_t last_served[64];
    if (system_time % 10000 == 0) {
        cerr << "[MEM " << m_id << " @ " << system_time << " ]";
        if (stats_enabled()) {
            cerr << " total served : " << stats()->total_served();
            cerr << " since last : " << stats()->total_served() - last_served[m_id];
            last_served[m_id] = stats()->total_served();
        }
        cerr << " in work table : " << m_work_table.size() << endl;
    }
#endif

    schedule_requests();

    m_l1->tick_positive_edge();
    m_l2->tick_positive_edge();
    m_cat->tick_positive_edge();
    if(m_dramctrl) {
        m_dramctrl->tick_positive_edge();
    }
}

void sharedSharedLCC::tick_negative_edge() {

    m_l1->tick_negative_edge();
    m_l2->tick_negative_edge();
    m_cat->tick_negative_edge();
    if(m_dramctrl) {
        m_dramctrl->tick_negative_edge();
    }

    /* accept messages and write into tables */
    accept_incoming_messages();

    update_work_table();

    update_dramctrl_work_table();

}

void sharedSharedLCC::update_work_table() {

    for (workTable::iterator it_addr = m_work_table.begin(); it_addr != m_work_table.end(); ) {

        maddr_t start_maddr = it_addr->first;
        shared_ptr<tableEntry>& entry = it_addr->second;

#if 0
        if (system_time > 600) {
            mh_log(4) << "[Mem " << m_id << " @ " << system_time << " ] in state " << entry->status 
                << " for " << start_maddr << endl;
        }
#endif

        shared_ptr<memoryRequest>& core_req = entry->core_req;
        shared_ptr<catRequest>& cat_req = entry->cat_req;

        shared_ptr<cacheRequest>& l1_req = entry->l1_req;
        shared_ptr<cacheRequest>& l2_req = entry->l2_req;
        shared_ptr<coherenceMsg>& remote_req = entry->remote_req;
        shared_ptr<coherenceMsg>& bypass_remote_req = entry->bypass_remote_req; 
        shared_ptr<memoryRequest>& bypass_core_req = entry->bypass_core_req; 
        shared_ptr<coherenceMsg>& remote_rep = entry->remote_rep; 
        shared_ptr<coherenceMsg>& remote_checkin = entry->remote_checkin; 
        shared_ptr<dramctrlMsg>& dramctrl_req = entry->dramctrl_req;
        shared_ptr<dramctrlMsg>& dramctrl_rep = entry->dramctrl_rep;

        shared_ptr<cacheLine> l1_line = (l1_req)? l1_req->line_copy() : shared_ptr<cacheLine>();
        shared_ptr<cacheLine> l2_line = (l2_req)? l2_req->line_copy() : shared_ptr<cacheLine>();
        shared_ptr<cacheLine> l1_victim = (l1_req)? l1_req->line_to_evict_copy() : shared_ptr<cacheLine>();
        shared_ptr<cacheLine> l2_victim = (l2_req)? l2_req->line_to_evict_copy() : shared_ptr<cacheLine>();
        shared_ptr<coherenceInfo> l1_line_info = 
            (l1_line)? static_pointer_cast<coherenceInfo>(l1_line->coherence_info) : shared_ptr<coherenceInfo>();
        shared_ptr<coherenceInfo> l2_line_info = 
            (l2_line)? static_pointer_cast<coherenceInfo>(l2_line->coherence_info) : shared_ptr<coherenceInfo>();
        shared_ptr<coherenceInfo> l1_victim_info = 
            (l1_victim)? static_pointer_cast<coherenceInfo>(l1_victim->coherence_info) : shared_ptr<coherenceInfo>();

        shared_ptr<sharedSharedLCCStatsPerMemInstr>& per_mem_instr_stats = entry->per_mem_instr_stats;

        if (entry->status == _CAT_AND_L1_FOR_LOCAL) {

            uint32_t home;
            uint64_t expired_amount = 0;

            if (l1_req->status() == CACHE_REQ_HIT) {

                home = l1_line_info->home;

                cat_req = shared_ptr<catRequest>();

                shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
                uint32_t word_offset = (core_req->maddr().address / 4 ) % m_cfg.words_per_cache_line;
                for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                    ret[i] = l1_line->data[i + word_offset];
                }
                set_req_data(core_req, ret);
                set_req_status(core_req, REQ_DONE);
                ++m_available_core_ports;

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a local L1 HIT and finish serving address "
                          << core_req->maddr() << endl;

                if (stats_enabled()) {
                    if (per_mem_instr_stats) {
                        /* sharedSharedLCCStatsPerMemInstr */
                        per_mem_instr_stats->get_tentative_data(T_IDX_L1)->add_l1_ops(system_time - l1_req->operation_begin_time());
                        per_mem_instr_stats->add_local_l1_cost_for_hit(per_mem_instr_stats->get_tentative_data(T_IDX_L1)->total_cost());
                        per_mem_instr_stats->commit_tentative_data(T_IDX_L1);
                        /* sharedSharedLCCStatsPerTile */
                        stats()->commit_per_mem_instr_stats(per_mem_instr_stats);
                    }
                    if (core_req->is_read()) {
                        stats()->hit_for_read_instr_at_local_l1();
                        if (m_id == home) {
                            stats()->hit_for_read_instr_at_home_l1();
                        } else {
                            stats()->hit_for_read_instr_at_away_l1();
                        }
                        stats()->did_finish_read();
                    } else {
                        stats()->hit_for_write_instr_at_local_l1();
                        if (m_id == home) {
                            stats()->hit_for_write_instr_at_home_l1();
                        } else {
                            stats()->hit_for_write_instr_at_away_l1();
                        }
                        stats()->did_finish_write();
                    }
                } else if (per_mem_instr_stats) {
                    per_mem_instr_stats->clear_tentative_data();
                }

                if (entry->using_read_exclusive_space) {
                    ++m_work_table_vacancy_read_exclusive;
                } else if (entry->using_send_checkin_exclusive_space) {
                    ++m_work_table_vacancy_send_checkin_exclusive;
                } else if (entry->using_receive_checkin_exclusive_space) {
                    ++m_work_table_vacancy_receive_checkin_exclusive;
                } else {
                    ++m_work_table_vacancy_shared;
                }
                m_work_table.erase(it_addr++);
                continue;
                /* FINISHED */

            } else if (l1_req->status() == CACHE_REQ_MISS && l1_line) {

                home = l1_line_info->home;
                cat_req = shared_ptr<catRequest>();

                if (per_mem_instr_stats) {
                    if (stats_enabled()) {
                        per_mem_instr_stats->get_tentative_data(T_IDX_L1)->add_l1_ops(system_time - l1_req->operation_begin_time());
                        per_mem_instr_stats->add_local_l1_cost_for_miss(per_mem_instr_stats->get_tentative_data(T_IDX_L1)->total_cost());
                        per_mem_instr_stats->commit_tentative_data(T_IDX_L1);
                    } else {
                        per_mem_instr_stats->clear_tentative_data();
                    }
                }

                if (home == m_id) {
                    if (l1_line_info->checked_out) {
                        /* checked out to someone */
                        mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a local L1 MISS for a checked out write copy "
                                  << "on address " << core_req->maddr() << endl;
                        if (stats_enabled()) {
                            if (core_req->is_read()) {
                                stats()->checkin_blocked_miss_for_read_instr_at_local_l1();
                                stats()->checkin_blocked_miss_for_read_instr_at_home_l1();
                            } else {
                                stats()->checkin_blocked_miss_for_write_instr_at_local_l1();
                                stats()->checkin_blocked_miss_for_write_instr_at_home_l1();
                            }
                        }
                        /* NOTE: if a checkin message is already arrived here, we lose one cycle (could be optimized later) */
                        entry->block_begin_time = system_time;
                        entry->status = _WAIT_CHECKIN;
                        entry->substatus = _WAIT_CHECKIN__L1;
                        ++it_addr;
                        continue;
                        /* TRANSITION */

                    } else {
                        /* timestamp unexpired for a write request */
                        mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a local L1 MISS for an unexpired timestamp "
                                  << "on address " << core_req->maddr() << endl;
                        if (core_req->is_read()) {
                        mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a local L1 MISS for an unexpired timestamp "
                                  << "on address " << core_req->maddr() << endl;
                        }
                           
                        mh_assert(!core_req->is_read());
                        if (stats_enabled()) {
                            stats()->ts_blocked_miss_for_write_instr_at_home_l1();
                            stats()->ts_blocked_miss_for_write_instr_at_local_l1();
                        }

                        entry->block_begin_time = system_time;
                        entry->blocking_timestamp = *l1_line_info->timestamp;
                        entry->blocked_data = l1_line->data;
                        entry->blocked_line_info = l1_line_info;
                        entry->status = _WAIT_TIMESTAMP;
                        entry->substatus = _WAIT_TIMESTAMP__L1;
                        entry->can_bypass_read_req = true;
                        ++it_addr;
                        continue;
                        /* TRANSITION */
                    }
                } else {
                    /* home != m_id */
                    mh_assert(!l1_line_info->checked_out); /* checkout line always hits */
                    if (core_req->is_read()) {
                        /* a read copy is expired */
                        mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a local L1 MISS as a read copy is expired "
                                  << "on address " << core_req->maddr() << endl;
                        expired_amount = system_time - *(l1_line_info->timestamp);
                        mh_assert(system_time > *(l1_line_info->timestamp));
                        if (stats_enabled()) {
                            stats()->ts_expired_miss_for_read_instr_at_away_l1();
                            stats()->ts_expired_miss_for_read_instr_at_local_l1();
                        }
                    } else {
                        /* a write attempted on a read copy */
                        mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a local L1 MISS (cannot write on a read copy) "
                                  << "on address " << core_req->maddr() << endl;
                        if (stats_enabled()) {
                            if (system_time < *(l1_line_info->timestamp)) {
                                stats()->permission_miss_ts_expired_for_write_instr_at_away_l1();
                                stats()->permission_miss_ts_expired_for_write_instr_at_local_l1();
                            } else {
                                stats()->permission_miss_ts_unexpired_for_write_instr_at_away_l1();
                                stats()->permission_miss_ts_unexpired_for_write_instr_at_local_l1();
                            }
                        }
                    }
                    /* continue to the end of the routine to send a remote request */
                }
            } else {
                /* record when CAT/L1 is finished */
                if (cat_req->operation_begin_time() != UINT64_MAX && cat_req->status() == CAT_REQ_DONE) {
                    if (stats_enabled() && per_mem_instr_stats) {
                        per_mem_instr_stats->get_tentative_data(T_IDX_CAT)->add_cat_ops(system_time - cat_req->operation_begin_time());
                    }
                    cat_req->set_operation_begin_time(UINT64_MAX);
                }

                if (l1_req->operation_begin_time() != UINT64_MAX && l1_req->status() == CACHE_REQ_MISS) {
                    if (stats_enabled() && per_mem_instr_stats) {
                        per_mem_instr_stats->get_tentative_data(T_IDX_L1)->add_l1_ops(system_time - l1_req->operation_begin_time());
                    }
                    l1_req->set_operation_begin_time(UINT64_MAX);
                }

                if (cat_req->status() == CAT_REQ_DONE) {
                    /* cannot continue without CAT info */
                    home = cat_req->home();

                    if (home == m_id) {
                        /* Core hit */
                        if (l1_req->status() == CACHE_REQ_MISS) {
                            /* Core hit, L1 true miss */
                            if (stats_enabled()) {
                                if (per_mem_instr_stats) {
                                    if (per_mem_instr_stats->get_max_tentative_data_index() == T_IDX_L1) {
                                        per_mem_instr_stats->add_local_l1_cost_for_miss
                                            (per_mem_instr_stats->get_tentative_data(T_IDX_L1)->total_cost());
                                    }
                                    per_mem_instr_stats->commit_max_tentative_data();
                                }
                                if (core_req->is_read()) {
                                    stats()->true_miss_for_read_instr_at_home_l1();
                                    stats()->true_miss_for_read_instr_at_local_l1();
                                    stats()->new_read_instr_at_l2();
                                } else {
                                    stats()->true_miss_for_write_instr_at_home_l1();
                                    stats()->true_miss_for_write_instr_at_local_l1();
                                    stats()->new_write_instr_at_l2();
                                }
                            } else if (per_mem_instr_stats) {
                                per_mem_instr_stats->clear_tentative_data();
                            }

                            mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a core hit but a true L1 MISS for "
                                      << core_req->maddr() << endl;

                            if (core_req->is_read()) {
                                l2_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_READ,
                                                                                   m_cfg.words_per_cache_line));
                            } else {
                                l2_req = shared_ptr<cacheRequest>(new cacheRequest(core_req->maddr(), CACHE_REQ_WRITE,
                                                                                   core_req->word_count(), core_req->data()));
                            }
                            l2_req->set_serialization_begin_time(system_time);
                            l2_req->set_unset_dirty_on_write(false);
                            l2_req->set_claim(false);
                            l2_req->set_evict(false);
                            l2_req->set_aux_info_for_coherence(
                                shared_ptr<auxInfoForCoherence>(new auxInfoForCoherence(m_id, m_id, core_req->is_read(), m_cfg))
                            );

                            if (l2_req->use_read_ports()) {
                                m_l2_read_req_schedule_q.push_back(entry);
                            } else {
                                m_l2_write_req_schedule_q.push_back(entry);
                            }

                            entry->status = _L2;

                            ++it_addr;
                            continue;
                            /* TRANSITION */
                        } else {
                            if (l1_req->status() == CACHE_REQ_NEW) {
                                if (l1_req->use_read_ports()) {
                                    m_l1_read_req_schedule_q.push_back(entry);
                                } else {
                                    m_l1_write_req_schedule_q.push_back(entry);
                                }
                            }
                            ++it_addr;
                            continue;
                            /* SPIN */
                        }
                    } else {
                        /* core miss */
                        if (!m_cfg.use_checkout_for_write_copy && !core_req->is_read()) {
                            /* If not W-LCC, an away write always misses */
                            mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a core miss for a write request "
                                      << "on address " << core_req->maddr() << endl;
                            l1_req = shared_ptr<cacheRequest>();
                            if (stats_enabled()) {
                                if (per_mem_instr_stats) {
                                    per_mem_instr_stats->commit_tentative_data(T_IDX_CAT);
                                }
                                stats()->true_miss_for_write_instr_at_away_l1();
                                stats()->true_miss_for_write_instr_at_local_l1();
                            } else if (per_mem_instr_stats) {
                                per_mem_instr_stats->clear_tentative_data();
                            }

                            if (m_cfg.migration_logic == MIGRATION_ALWAYS_ON_WRITES) {
                                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] need to migrate to " << home 
                                          << "for address " << core_req->maddr() << endl;
                                if (per_mem_instr_stats) {
                                    per_mem_instr_stats->migration_started(system_time);
                                }
                                set_req_status(core_req, REQ_MIGRATE);
                                set_req_home(core_req, home);
                                ++m_available_core_ports;

                                if (entry->using_read_exclusive_space) {
                                    ++m_work_table_vacancy_read_exclusive;
                                } else if (entry->using_send_checkin_exclusive_space) {
                                    ++m_work_table_vacancy_send_checkin_exclusive;
                                } else if (entry->using_receive_checkin_exclusive_space) {
                                    ++m_work_table_vacancy_receive_checkin_exclusive;
                                } else {
                                    ++m_work_table_vacancy_shared;
                                }
                                m_work_table.erase(it_addr++);
                                continue;
                                /* FINISHED */
                            }
                            /* continue to the end of the routine to send a remote request */
                        } else if (l1_req->status() == CACHE_REQ_MISS) {
                            mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a core miss AND L1 true miss "
                                      << "on address " << core_req->maddr() << endl;
                            if (stats_enabled()) {
                                if (per_mem_instr_stats) {
                                    if (per_mem_instr_stats->get_max_tentative_data_index() == T_IDX_L1) {
                                        per_mem_instr_stats->add_local_l1_cost_for_miss
                                            (per_mem_instr_stats->get_tentative_data(T_IDX_L1)->total_cost());
                                    }
                                    per_mem_instr_stats->commit_max_tentative_data();
                                }
                                if (core_req->is_read()) {
                                    stats()->true_miss_for_read_instr_at_away_l1();
                                    stats()->true_miss_for_read_instr_at_local_l1();
                                } else {
                                    stats()->true_miss_for_write_instr_at_away_l1();
                                    stats()->true_miss_for_write_instr_at_local_l1();
                                }
                            } else if (per_mem_instr_stats) {
                                per_mem_instr_stats->clear_tentative_data();
                            }
                            /* continue to the end of the routine to send a remote request */
                        } else {
                            if (l1_req->status() == CACHE_REQ_NEW) {
                                if (l1_req->use_read_ports()) {
                                    m_l1_read_req_schedule_q.push_back(entry);
                                } else {
                                    m_l1_write_req_schedule_q.push_back(entry);
                                }
                            }
                            ++it_addr;
                            continue;
                            /* SPIN */
                        }
                    }
                } else {
                    /* CAT not ready */

                    if (l1_req->status() == CACHE_REQ_NEW) {
                        if (l1_req->use_read_ports()) {
                            m_l1_read_req_schedule_q.push_back(entry);
                        } else {
                            m_l1_write_req_schedule_q.push_back(entry);
                        }
                    }

                    if (cat_req->status() == CAT_REQ_NEW) {
                        m_cat_req_schedule_q.push_back(entry);
                    }
                    ++it_addr;
                    continue;
                    /* SPIN */
                }
            }

            /* reach here only when sending a remote request */
            if (stats_enabled() && per_mem_instr_stats) {
                per_mem_instr_stats->core_missed();
            }

            remote_req = shared_ptr<coherenceMsg>(new coherenceMsg);
            remote_req->sender = m_id;
            remote_req->receiver = home;
            remote_req->type = (core_req->is_read())? REMOTE_READ_REQ : REMOTE_WRITE_REQ;
            remote_req->word_count = core_req->word_count();
            remote_req->maddr = core_req->maddr();
            remote_req->data = (core_req->is_read())? shared_array<uint32_t>() : core_req->data();
            remote_req->coherence_info = shared_ptr<coherenceInfo>();
            remote_req->sent = false;
            remote_req->per_mem_instr_stats = per_mem_instr_stats;
            remote_req->birthtime = system_time;
            remote_req->expired_amount = 0;

            m_remote_req_schedule_q.push_back(entry);

            entry->status = _SEND_REMOTE_REQ;
            ++it_addr;
            continue;
            /* TRANSITION */
        } else if (entry->status == _L1_FOR_REMOTE) {

            if (l1_req->status() == CACHE_REQ_NEW) {
                /* lost last arbitration */
                if (l1_req->use_read_ports()) {
                    m_l1_read_req_schedule_q.push_back(entry);
                } else {
                    m_l1_write_req_schedule_q.push_back(entry);
                }
                ++it_addr;
                continue;
                /* SPIN */
            } else if (l1_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
                /* SPIN */
            }

            if (per_mem_instr_stats) {
                if (stats_enabled()) {
                    per_mem_instr_stats->get_tentative_data(T_IDX_L1)->add_l1_ops(system_time - l1_req->serialization_begin_time());
                    if (l1_req->status() == CACHE_REQ_HIT) {
                        per_mem_instr_stats->add_remote_l1_cost_for_hit(per_mem_instr_stats->get_tentative_data(T_IDX_L1)->total_cost());
                    } else {
                        per_mem_instr_stats->add_remote_l1_cost_for_miss(per_mem_instr_stats->get_tentative_data(T_IDX_L1)->total_cost());
                    }
                    per_mem_instr_stats->commit_tentative_data(T_IDX_L1);
                } else {
                    per_mem_instr_stats->clear_tentative_data();
                }
            }

            if (l1_req->status() == CACHE_REQ_HIT) {

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets an L1 HIT for a remote request on "
                          << remote_req->maddr << endl;

                if (stats_enabled()) {
                    if (remote_req->type == REMOTE_READ_REQ) {
                        stats()->hit_for_read_instr_at_home_l1();
                        stats()->hit_for_read_instr_at_remote_l1();
                    } else {
                        stats()->hit_for_write_instr_at_home_l1();
                        stats()->hit_for_write_instr_at_remote_l1();
                    }
                }

                remote_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
                remote_rep->sender = m_id;
                remote_rep->sent = false;
                remote_rep->type = REMOTE_REP;
                remote_rep->receiver = remote_req->sender;
                if (remote_req->type == REMOTE_READ_REQ) {
                    remote_rep->word_count = remote_req->word_count;
                } else {
                    remote_rep->word_count = 0;
                }
                remote_rep->maddr = remote_req->maddr;
                remote_rep->data = l1_line->data;
                remote_rep->birthtime = system_time;
                remote_rep->coherence_info = l1_line_info; /* l1_line_info is not used here, so need not copy */

                entry->status = _SEND_REMOTE_REP;
                entry->substatus = _SEND_REMOTE_REP__CACHE;

                m_remote_rep_and_checkin_schedule_q.push_back(make_tuple(true,entry));

                ++it_addr;

                continue;
                /* TRANSITION */

            } else if (l1_req->status() == CACHE_REQ_MISS) {
                if (l1_line) {
                    if (l1_line_info->checked_out) {
                        /* checked out to someone */
                        mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets an L1 MISS for a remote request "
                                  << "for a checked out write copy on address " << remote_req->maddr << endl;
                        if (stats_enabled()) {
                            if (l1_req->request_type() == CACHE_REQ_READ) {
                                stats()->checkin_blocked_miss_for_read_instr_at_home_l1();
                                stats()->checkin_blocked_miss_for_read_instr_at_remote_l1();
                            } else {
                                stats()->checkin_blocked_miss_for_write_instr_at_home_l1();
                                stats()->checkin_blocked_miss_for_write_instr_at_remote_l1();
                            }
                        }
                        /* if a checkin message is already arrived here, we lose one cycle (could be optimized later) */
                        entry->block_begin_time = system_time;
                        entry->status = _WAIT_CHECKIN;
                        entry->substatus = _WAIT_CHECKIN__L1;
                        ++it_addr;
                        continue;
                        /* TRANSITION */

                    } else {
                        /* timestamp unexpired for a write request */
                        mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets an L1 MISS for a remote request "
                                  << "for an unexpired timestamp " << remote_req->maddr << endl;
                        mh_assert(remote_req->type == REMOTE_WRITE_REQ);
                        if (stats_enabled()) {
                            stats()->ts_blocked_miss_for_write_instr_at_home_l1();
                            stats()->ts_blocked_miss_for_write_instr_at_remote_l1();
                        }

                        entry->block_begin_time = system_time;
                        entry->blocking_timestamp = *l1_line_info->timestamp;
                        entry->blocked_data = l1_line->data;
                        entry->blocked_line_info = l1_line_info;
                        entry->status = _WAIT_TIMESTAMP;
                        entry->substatus = _WAIT_TIMESTAMP__L1;
                        entry->can_bypass_read_req = true;
                        ++it_addr;
                        continue;
                        /* TRANSITION */
                    }
                } else {
                    /* !l1_line - true miss */
                    if (stats_enabled()) {
                        if (remote_req->type == REMOTE_READ_REQ) {
                            stats()->true_miss_for_read_instr_at_home_l1();
                            stats()->true_miss_for_read_instr_at_remote_l1();
                            stats()->new_read_instr_at_l2();
                        } else {
                            stats()->true_miss_for_write_instr_at_home_l1();
                            stats()->true_miss_for_write_instr_at_remote_l1();
                            stats()->new_write_instr_at_l2();
                        }
                    }

                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a true L1 MISS for remote req on "
                              << remote_req->maddr << endl;

                    if (remote_req->type == REMOTE_READ_REQ) {
                        l2_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_READ,
                                                                           m_cfg.words_per_cache_line));
                    } else {
                        l2_req = shared_ptr<cacheRequest>(new cacheRequest(remote_req->maddr, CACHE_REQ_WRITE,
                                                                           remote_req->word_count, remote_req->data));
                    }
                    l2_req->set_serialization_begin_time(system_time);
                    l2_req->set_unset_dirty_on_write(false);
                    l2_req->set_claim(false);
                    l2_req->set_evict(false);
                    l2_req->set_aux_info_for_coherence(
                        shared_ptr<auxInfoForCoherence>(new auxInfoForCoherence(m_id, 
                                                                                remote_req->sender, 
                                                                                remote_req->type == REMOTE_READ_REQ, m_cfg))
                    );

                    if (l2_req->use_read_ports()) {
                        m_l2_read_req_schedule_q.push_back(entry);
                    } else {
                        m_l2_write_req_schedule_q.push_back(entry);
                    }

                    entry->status = _L2;

                    ++it_addr;
                    continue;
                    /* TRANSITION */
                }
            }
            /* _L1_FOR_REMOTE - never reach here */
        } else if (entry->status == _L2) {
            if (l2_req->status() == CACHE_REQ_NEW) {
                /* lost last arbitration */
                if (l2_req->use_read_ports()) {
                    m_l2_read_req_schedule_q.push_back(entry);
                } else {
                    m_l2_write_req_schedule_q.push_back(entry);
                }
                ++it_addr;
                continue;
                /* SPIN */
            } else if (l2_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
                /* SPIN */
            } 

            if (per_mem_instr_stats) {
                if (stats_enabled()) {
                    per_mem_instr_stats->get_tentative_data(T_IDX_L2)->add_l2_ops(system_time - l2_req->serialization_begin_time());
                    if (l2_req->status() == CACHE_REQ_HIT) {
                        if (core_req) {
                            per_mem_instr_stats->add_local_l2_cost_for_hit(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                        } else {
                            per_mem_instr_stats->add_remote_l2_cost_for_hit(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                        }
                    } else {
                        if (core_req) {
                            per_mem_instr_stats->add_local_l2_cost_for_miss(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                        } else {
                            per_mem_instr_stats->add_remote_l2_cost_for_miss(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                        }
                    }
                    per_mem_instr_stats->commit_tentative_data(T_IDX_L2);
                } else {
                    per_mem_instr_stats->clear_tentative_data();
                }
            }

            if (l2_req->status() == CACHE_REQ_HIT) {

                if (stats_enabled()) {
                    if ((core_req && core_req->is_read()) || (remote_req && remote_req->type == REMOTE_READ_REQ)) {
                        stats()->hit_for_read_instr_at_l2();
                        if (core_req) {
                            stats()->hit_for_read_instr_at_local_l2();
                        } else {
                            stats()->hit_for_read_instr_at_remote_l2();
                        }
                    } else {
                        stats()->hit_for_write_instr_at_l2();
                        if (core_req) {
                            stats()->hit_for_write_instr_at_local_l2();
                        } else {
                            stats()->hit_for_write_instr_at_remote_l2();
                        }
                    }
                }

                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] gets an L2 HIT for a request on "
                          << l2_req->maddr() << endl;

                l1_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                                   m_cfg.words_per_cache_line,
                                                                   l2_line->data, l2_line_info));
                l1_req->set_serialization_begin_time(system_time);
                l1_req->set_unset_dirty_on_write(true);
                l1_req->set_claim(true);
                l1_req->set_evict(true);

                if (l1_req->use_read_ports()) {
                    m_l1_read_req_schedule_q.push_back(entry);
                } else {
                    m_l1_write_req_schedule_q.push_back(entry);
                }

                entry->status = _UPDATE_L1;

                ++it_addr;
                continue;
                /* TRANSITION */

            } else {
                /* miss */
                if (!l2_line) {
                    /* true miss */
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] gets a true L2 MISS on address "
                              << l2_req->maddr() << endl;
                    if (stats_enabled()) {
                        if ((core_req && core_req->is_read()) || (remote_req && remote_req->type == REMOTE_READ_REQ)) {
                            stats()->true_miss_for_read_instr_at_l2();
                            if (core_req) {
                                stats()->true_miss_for_read_instr_at_local_l2();
                            } else {
                                stats()->true_miss_for_read_instr_at_remote_l2();
                            }
                        } else {
                            stats()->true_miss_for_write_instr_at_l2();
                            if (core_req) {
                                stats()->true_miss_for_write_instr_at_local_l2();
                            } else {
                                stats()->true_miss_for_write_instr_at_remote_l2();
                            }
                        }
                    }

                    dramctrl_req = shared_ptr<dramctrlMsg>(new dramctrlMsg);
                    dramctrl_req->sender = m_id;
                    dramctrl_req->receiver = m_dramctrl_location;
                    dramctrl_req->maddr = start_maddr;
                    dramctrl_req->dram_req = shared_ptr<dramRequest>(new dramRequest(start_maddr,
                                                                                     DRAM_REQ_READ,
                                                                                     m_cfg.words_per_cache_line));
                    dramctrl_req->sent = false;
                    dramctrl_req->birthtime = system_time;
                    dramctrl_req->per_mem_instr_stats = per_mem_instr_stats;

                    m_dramctrl_req_schedule_q.push_back(make_tuple(false, entry)); /* mark it as from local */

                    entry->status = _SEND_DRAMCTRL_REQ;

                    ++it_addr;
                    continue;
                    /* TRANSITION */
                } else {
                    if (l2_line_info->checked_out) {
                        /* checkin blocked */
                        mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] gets a L2 MISS for a checked out write copy "
                                  << "on address " << l2_req->maddr() << endl;
                        if (stats_enabled()) {
                            if ((core_req && core_req->is_read()) || (remote_req && remote_req->type == REMOTE_READ_REQ)) {
                                stats()->checkin_blocked_miss_for_read_instr_at_l2();
                                if (core_req) {
                                    stats()->checkin_blocked_miss_for_read_instr_at_local_l2();
                                } else {
                                    stats()->checkin_blocked_miss_for_read_instr_at_remote_l2();
                                }

                            } else {
                                stats()->checkin_blocked_miss_for_write_instr_at_l2();
                                if (core_req) {
                                    stats()->checkin_blocked_miss_for_write_instr_at_local_l2();
                                } else {
                                    stats()->checkin_blocked_miss_for_write_instr_at_remote_l2();
                                }
                            }
                        }
                        /* NOTE: if a checkin message is already arrived here, we lose one cycle (could be optimized later) */
                        entry->block_begin_time = system_time;
                        entry->status = _WAIT_CHECKIN;
                        entry->substatus = _WAIT_CHECKIN__L2;
                        ++it_addr;
                        continue;
                        /* TRANSITION */

                    } else {
                        /* unexpired TS */
                        mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] gets a L2 MISS for an unexpired timestamp "
                                  << "on address " << l2_req->maddr() << endl;
                        mh_assert((core_req && !core_req->is_read()) || (remote_req && remote_req->type == REMOTE_WRITE_REQ));
                        if (stats_enabled()) {
                            stats()->ts_blocked_miss_for_write_instr_at_l2();
                            if (core_req) {
                                stats()->ts_blocked_miss_for_write_instr_at_local_l2();
                            } else {
                                stats()->ts_blocked_miss_for_write_instr_at_remote_l2();
                            }
                        }

                        entry->block_begin_time = system_time;
                        entry->blocking_timestamp = *l2_line_info->timestamp;
                        entry->blocked_data = l2_line->data;
                        entry->blocked_line_info = l2_line_info;
                        entry->status = _WAIT_TIMESTAMP;
                        entry->substatus = _WAIT_TIMESTAMP__L2;
                        entry->can_bypass_read_req = true;
                        ++it_addr;
                        continue;
                        /* TRANSITION */
                    }
                }
            }
            /* _L2 - never reach here */
        } else if (entry->status == _WAIT_CHECKIN) {
            if (!remote_checkin) {
                ++it_addr;
                continue;
                /* SPIN */
            }
            if (stats_enabled()) {
                if (per_mem_instr_stats) {
                    if (entry->substatus == _WAIT_CHECKIN__L1) {
                        per_mem_instr_stats->add_l1_blk_by_checkin(system_time - entry->block_begin_time);
                        if (core_req) {
                            per_mem_instr_stats->add_local_l1_blk_by_checkin(system_time - entry->block_begin_time);
                        } else {
                            per_mem_instr_stats->add_local_l1_blk_by_checkin(system_time - entry->block_begin_time);
                        }
                    } else {
                        per_mem_instr_stats->add_l2_blk_by_checkin(system_time - entry->block_begin_time);
                        if (core_req) {
                            per_mem_instr_stats->add_local_l2_blk_by_checkin(system_time - entry->block_begin_time);
                        } else {
                            per_mem_instr_stats->add_local_l2_blk_by_checkin(system_time - entry->block_begin_time);
                        }
                    }
                }
            }
            mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] received a checkin "
                      << "on address " << start_maddr << endl;

            mh_assert(m_handler_for_remote_checkin);
            shared_ptr<auxInfoForCoherence> req_info(
                new auxInfoForCoherence(m_id, (core_req)? m_id : remote_req->sender,
                                        (core_req && core_req->is_read()) || (remote_req && remote_req->type==REMOTE_READ_REQ),
                                        m_cfg)
            );
            (*m_handler_for_remote_checkin)(remote_checkin->coherence_info, req_info, system_time);

            if (entry->substatus == _WAIT_CHECKIN__L1) {
                l1_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                                   (remote_checkin->data)? m_cfg.words_per_cache_line : 0,
                                                                   (remote_checkin->data)? remote_checkin->data:shared_array<uint32_t>(),
                                                                   remote_checkin->coherence_info));
                l1_req->set_serialization_begin_time(system_time);
                l1_req->set_unset_dirty_on_write(false); 
                l1_req->set_claim(true);
                l1_req->set_evict(true);

                entry->status = _UPDATE_L1;

                ++it_addr;
                continue;
                /* TRANSITION */
            } else {
                l2_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                                   (remote_checkin->data)? m_cfg.words_per_cache_line : 0,
                                                                   (remote_checkin->data)? remote_checkin->data:shared_array<uint32_t>(),
                                                                   remote_checkin->coherence_info));
                l2_req->set_serialization_begin_time(system_time);
                l2_req->set_unset_dirty_on_write(false); 
                l2_req->set_claim(true);
                l2_req->set_evict(true);

                entry->status = _UPDATE_L2;

                ++it_addr;
                continue;
                /* TRANSITION */
            }
 
        } else if (entry->status == _SEND_REMOTE_REQ) {
            if (remote_req->sent) {
                entry->status = _WAIT_REMOTE_REP;
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] sent an REMOTE request (" << remote_req->type
                          << ") to " << remote_req->receiver << " for " << remote_req->maddr << endl;
                ++it_addr;
                continue;
                /* TRANSITION */
            } else {
                m_remote_req_schedule_q.push_back(entry);
                ++it_addr;
                continue;
                /* SPIN */
            }
            /* _SEND_RA_REQ - never reach here */
        } else if (entry->status == _WAIT_REMOTE_REP) {
            if (remote_rep) {
                if (stats_enabled() && per_mem_instr_stats) {
                    per_mem_instr_stats->add_remote_rep_nas(system_time - remote_rep->birthtime);
                }

                bool cacheable = (remote_req->type == REMOTE_READ_REQ && *(remote_rep->coherence_info->timestamp) > system_time)
                                 || (remote_rep->coherence_info->checked_out);

                if (!cacheable) {
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] received a UNcacheable remote reply on "
                              << core_req->maddr() << endl;
                    shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
                    uint32_t word_offset = (core_req->maddr().address / 4 ) % m_cfg.words_per_cache_line;
                    for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                        ret[i] = remote_rep->data[i + word_offset];
                    }
                    set_req_data(core_req, ret);
                    if (stats_enabled()) {
                        if (per_mem_instr_stats) {
                            stats()->commit_per_mem_instr_stats(per_mem_instr_stats);
                        }
                        if (core_req->is_read()) {
                            stats()->did_finish_read();
                        } else {
                            stats()->did_finish_write();
                        }
                    }

                    set_req_status(core_req, REQ_DONE);
                    ++m_available_core_ports;

                    if (entry->using_read_exclusive_space) {
                        ++m_work_table_vacancy_read_exclusive;
                    } else if (entry->using_send_checkin_exclusive_space) {
                        ++m_work_table_vacancy_send_checkin_exclusive;
                    } else if (entry->using_receive_checkin_exclusive_space) {
                        ++m_work_table_vacancy_receive_checkin_exclusive;
                    } else {
                        ++m_work_table_vacancy_shared;
                    }

                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] finish serving address "
                              << core_req->maddr() << endl;

                    m_work_table.erase(it_addr++);
                    continue;
                    /* FINISH */
                } else {
                    /* cacheable */
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] received a cacheable remote reply on "
                              << core_req->maddr();
                    if (remote_rep->coherence_info->checked_out) {
                        mh_log(4) << " (CHECKED OUT)";
                    }
                    mh_log(4) << " (TIMESTAMP: " << *(remote_rep->coherence_info->timestamp) << " ) "  << endl;
                    mh_assert(m_handler_for_remote_rep);
                    shared_ptr<auxInfoForCoherence> req_info(new auxInfoForCoherence(m_id, m_id, core_req->is_read(), m_cfg));
                    (*m_handler_for_remote_rep)(remote_rep->coherence_info, req_info, system_time);

                    if (remote_rep->coherence_info->checked_out && *(remote_rep->coherence_info->timestamp) <= system_time) {
                        mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] cannot cache a checked-out line as expired on "
                                  << core_req->maddr() << endl;
                        remote_checkin = shared_ptr<coherenceMsg>(new coherenceMsg);
                        remote_checkin->sender = m_id;
                        remote_checkin->sent = false;
                        remote_checkin->type = REMOTE_CHECKIN;
                        remote_checkin->receiver = remote_rep->sender;
                        remote_checkin->word_count = 0;
                        remote_checkin->data = shared_array<uint32_t>();
                        remote_checkin->maddr = start_maddr;
                        remote_checkin->birthtime = system_time;
                        remote_checkin->coherence_info = remote_rep->coherence_info;

                        m_remote_rep_and_checkin_schedule_q.push_back(make_tuple(false, entry));

                        entry->status = _SEND_CHECKIN;
                        entry->substatus = _SEND_CHECKIN__UNCACHEABLE;

                        shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
                        uint32_t word_offset = (core_req->maddr().address / 4 ) % m_cfg.words_per_cache_line;
                        for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                            ret[i] = remote_rep->data[i + word_offset];
                        }
                        set_req_data(core_req, ret);

                        ++it_addr;
                        continue;
                        /* TRANSITION */
                    }

                    l1_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                                       m_cfg.words_per_cache_line,
                                                                       remote_rep->data,
                                                                       remote_rep->coherence_info));
                    l1_req->set_serialization_begin_time(system_time);
                    l1_req->set_unset_dirty_on_write(true);
                    l1_req->set_claim(true);
                    l1_req->set_evict(true);

                    if (l1_req->use_read_ports()) {
                        m_l1_read_req_schedule_q.push_back(entry);
                    } else {
                        m_l1_write_req_schedule_q.push_back(entry);
                    }

                    if (remote_rep->coherence_info->checked_out) {
                        m_scheduled_checkin.push_back(make_tuple(remote_rep->coherence_info->timestamp, start_maddr));
                    }

                    entry->status = _UPDATE_L1;

                    ++it_addr;
                    continue;
                    /* TRANSITION */
                }
            } else {
                ++it_addr;
                continue;
                /* SPIN */
            }
            /* _WAIT_RA_REP - never reach here */
        } else if (entry->status == _WAIT_TIMESTAMP) {

            if (system_time > entry->blocking_timestamp) {
                entry->can_bypass_read_req = false;
            }

            if (bypass_core_req) {
                mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] gets a local bypass HIT and finish serving address "
                          << bypass_core_req->maddr() << endl;

                shared_array<uint32_t> ret(new uint32_t[bypass_core_req->word_count()]);
                uint32_t word_offset = (bypass_core_req->maddr().address / 4 ) % m_cfg.words_per_cache_line;
                for (uint32_t i = 0; i < bypass_core_req->word_count(); ++i) {
                    ret[i] = entry->blocked_data[i + word_offset];
                }
                set_req_data(bypass_core_req, ret);
                set_req_status(bypass_core_req, REQ_DONE);
                ++m_available_core_ports;

                if (stats_enabled()) {
                    if (bypass_core_req->per_mem_instr_runtime_info()) {
                        shared_ptr<sharedSharedLCCStatsPerMemInstr> bypass_per_mem_instr_stats =
                            static_pointer_cast<sharedSharedLCCStatsPerMemInstr>(*bypass_core_req->per_mem_instr_runtime_info());
                        bypass_per_mem_instr_stats->add_bypass(system_time - entry->bypass_core_req_begin_time);
                        bypass_per_mem_instr_stats->add_local_bypass(system_time - entry->bypass_core_req_begin_time);
                        stats()->commit_per_mem_instr_stats(bypass_per_mem_instr_stats);
                    }
                    stats()->bypass_for_read_instr(true/*local*/);
                    stats()->did_finish_read();
                }
                bypass_core_req = shared_ptr<memoryRequest>();
                ++it_addr;
                continue;
                /* SPIN */
            }
            if (bypass_remote_req) {
                mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] gets a bypass HIT for remote read request on "
                          << bypass_remote_req->maddr << " for core " << bypass_remote_req->sender << endl;

                if (stats_enabled()) {
                    shared_ptr<sharedSharedLCCStatsPerMemInstr> bypass_per_mem_instr_stats = bypass_remote_req->per_mem_instr_stats;
                    if (bypass_per_mem_instr_stats) {
                        bypass_per_mem_instr_stats->add_bypass(system_time - entry->bypass_remote_req_begin_time);
                        bypass_per_mem_instr_stats->add_remote_bypass(system_time - entry->bypass_core_req_begin_time);
                    }
                    stats()->bypass_for_read_instr(false/*remote*/);
                }

                remote_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
                remote_rep->sender = m_id;
                remote_rep->sent = false;
                remote_rep->type = REMOTE_REP;
                remote_rep->receiver = bypass_remote_req->sender;
                remote_rep->word_count = bypass_remote_req->word_count;
                remote_rep->maddr = bypass_remote_req->maddr;
                remote_rep->data = entry->blocked_data;
                remote_rep->birthtime = system_time;
                remote_rep->coherence_info = static_pointer_cast<coherenceInfo>((*m_copy_coherence_info)(entry->blocked_line_info));

                entry->status = _SEND_REMOTE_REP;
                entry->substatus = (entry->substatus == _WAIT_TIMESTAMP__L1)? _SEND_REMOTE_REP__BYPASS_L1 : _SEND_REMOTE_REP__BYPASS_L2;

                bypass_remote_req = shared_ptr<coherenceMsg>();

                ++it_addr;
                continue;
                /* TRANSITION */
            }

            if (system_time <= entry->blocking_timestamp) {
                ++it_addr;
                continue;
                /* SPIN */
            }

            mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] unblocks a write request as the timestamp expires on address "
                      << start_maddr << endl;

            /* green to go */
            if (stats_enabled() && per_mem_instr_stats) {
                if (entry->substatus == _WAIT_TIMESTAMP__L1) {
                    per_mem_instr_stats->add_l1_blk_by_ts(system_time - entry->block_begin_time);
                    if (core_req) {
                        per_mem_instr_stats->add_local_l1_blk_by_ts(system_time - entry->block_begin_time);
                    } else {
                        per_mem_instr_stats->add_local_l1_blk_by_ts(system_time - entry->block_begin_time);
                    }
                } else {
                    per_mem_instr_stats->add_l2_blk_by_ts(system_time - entry->block_begin_time);
                    if (core_req) {
                        per_mem_instr_stats->add_local_l2_blk_by_ts(system_time - entry->block_begin_time);
                    } else {
                        per_mem_instr_stats->add_local_l2_blk_by_ts(system_time - entry->block_begin_time);
                    }
                }
            }
            maddr_t maddr = (core_req)? core_req->maddr() : remote_req->maddr;
            uint32_t word_count = (core_req)? core_req->word_count() : remote_req->word_count;
            shared_array<uint32_t> data = (core_req)? core_req->data() : remote_req->data;
            uint32_t word_offset = (maddr.address / 4) % m_cfg.words_per_cache_line;

            mh_assert(entry->blocked_data);
            mh_assert(data);
            mh_assert(word_offset + word_count <= m_cfg.words_per_cache_line);
            for (uint32_t i = 0; i < word_count; ++i) {
                entry->blocked_data[i + word_offset] = data[i];
            }

            mh_assert((core_req && !core_req->is_read()) || (remote_req && remote_req->type==REMOTE_WRITE_REQ));
            mh_assert(m_handler_for_waited_for_timestamp);
            shared_ptr<auxInfoForCoherence> req_info(new auxInfoForCoherence(m_id, 
                                                                             (core_req)? m_id : remote_req->sender, 
                                                                             false, m_cfg));

            if (entry->substatus == _WAIT_TIMESTAMP__L1) {

                (*m_handler_for_waited_for_timestamp)(entry->blocked_line_info, req_info, system_time);

                maddr_t maddr = (core_req)? core_req->maddr() : remote_req->maddr;
                uint32_t word_count = (core_req)? core_req->word_count() : remote_req->word_count;
                shared_array<uint32_t> data = (core_req)? core_req->data() : remote_req->data;
                uint32_t word_offset = ( maddr.address / 4 )  % m_cfg.words_per_cache_line;
                for (uint32_t i = 0; i < word_count; ++i) {
                    entry->blocked_data[i + word_offset] = data[i];
                }

                l1_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                                   m_cfg.words_per_cache_line,
                                                                   entry->blocked_data,
                                                                   entry->blocked_line_info 
                                                                   /* line info is always necessary for the case of an eviction */
                                                                   ));
                l1_req->set_serialization_begin_time(system_time);
                l1_req->set_unset_dirty_on_write(false);
                l1_req->set_claim(true);
                l1_req->set_evict(true);

                if (l1_req->use_read_ports()) {
                    m_l1_read_req_schedule_q.push_back(entry);
                } else {
                    m_l1_write_req_schedule_q.push_back(entry);
                }
                entry->status = _UPDATE_L1;
                ++it_addr;
                continue;
                /* TRANSITION */

            } else {
                (*m_handler_for_waited_for_timestamp)(entry->blocked_line_info, req_info, system_time);

                maddr_t maddr = (core_req)? core_req->maddr() : remote_req->maddr;
                uint32_t word_count = (core_req)? core_req->word_count() : remote_req->word_count;
                shared_array<uint32_t> data = (core_req)? core_req->data() : remote_req->data;
                uint32_t word_offset = ( maddr.address / 4 )  % m_cfg.words_per_cache_line;
                for (uint32_t i = 0; i < word_count; ++i) {
                    entry->blocked_data[i + word_offset] = data[i];
                }

                l2_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                                   m_cfg.words_per_cache_line,
                                                                   entry->blocked_data,
                                                                   entry->blocked_line_info 
                                                                   /* line info is always necessary for the case of an eviction */
                                                                  ));
                l2_req->set_serialization_begin_time(system_time);
                l2_req->set_unset_dirty_on_write(false);
                l2_req->set_claim(true);
                l2_req->set_evict(true);


                if (l2_req->use_read_ports()) {
                    m_l2_read_req_schedule_q.push_back(entry);
                } else {
                    m_l2_write_req_schedule_q.push_back(entry);
                }
                entry->status = _UPDATE_L2;
                ++it_addr;
                continue;
                /* TRANSITION */
            }
            /* _WAIT_TIMESTAMP - never reach here */

        } else if (entry->status == _SEND_REMOTE_REP) {
            if (!remote_rep->sent) {
                m_remote_rep_and_checkin_schedule_q.push_back(make_tuple(true,entry));
                ++it_addr;
                continue;
                /* SPIN */
            }

            mh_log(4) << "[Mem " << m_id << " @ " << system_time << " ] sent an REMOTE rep for " << remote_rep->maddr
                      << " to " << remote_rep->receiver;
            if (remote_rep->coherence_info->checked_out) {
                mh_log(4) << " (CHECKED OUT) ";
            }
            mh_log(4) << "(TIMESTAMP: " << *remote_rep->coherence_info->timestamp << " ) " << endl;

            if (entry->substatus == _SEND_REMOTE_REP__CACHE) {
                /* substatus : __CACHE */
                if (entry->using_read_exclusive_space) {
                    ++m_work_table_vacancy_read_exclusive;
                } else if (entry->using_send_checkin_exclusive_space) {
                    ++m_work_table_vacancy_send_checkin_exclusive;
                } else if (entry->using_receive_checkin_exclusive_space) {
                    ++m_work_table_vacancy_receive_checkin_exclusive;
                } else {
                    ++m_work_table_vacancy_shared;
                }
                m_work_table.erase(it_addr++);
                continue;
                /* FINISH */
            } else {
                /* substatus : __BYPASS */
                /* NOTE: could check additional bypassed requests or timestamp expiration  to earn one cycles here */
                /*       (as shown in the FSM chart) */
                /*       not doing it for now.  */
                entry->status = _WAIT_TIMESTAMP;
                if (entry->substatus == _SEND_REMOTE_REP__BYPASS_L1) {
                    entry->substatus = _WAIT_TIMESTAMP__L1;
                } else {
                    entry->substatus = _WAIT_TIMESTAMP__L2;
                }
                ++it_addr;
                continue;
                /* TRANSITION */
            }
            /* _SEND_REMOTE_REP - never reach here */
        } else if (entry->status == _UPDATE_L1) {

            if (l1_req->status() == CACHE_REQ_NEW) {
                /* lost last arbitration */
                if (l1_req->use_read_ports()) {
                    m_l1_read_req_schedule_q.push_back(entry);
                } else {
                    m_l1_write_req_schedule_q.push_back(entry);
                }
                ++it_addr;
                continue;
                /* SPIN */
            } else if (l1_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
                /* SPIN */
            } 

            if (per_mem_instr_stats) {
                if (stats_enabled()) {
                    per_mem_instr_stats->get_tentative_data(T_IDX_L1)->add_l1_ops(system_time - l1_req->serialization_begin_time());
                    if (remote_rep) {
                        per_mem_instr_stats->add_remote_l1_cost_for_update(per_mem_instr_stats->get_tentative_data(T_IDX_L1)->total_cost());
                    } else {
                        per_mem_instr_stats->add_local_l1_cost_for_update(per_mem_instr_stats->get_tentative_data(T_IDX_L1)->total_cost());
                    }
                    per_mem_instr_stats->commit_tentative_data(T_IDX_L1);
                } else {
                    per_mem_instr_stats->clear_tentative_data();
                }
            }

            if (l1_req->status() == CACHE_REQ_MISS) {
                /* could not evict any line - must wait(retry) */
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] could not update for cache full on address "
                          << l1_req->maddr() << endl;
                l1_req->set_serialization_begin_time(system_time);
                l1_req->reset();
                if (l1_req->use_read_ports()) {
                    m_l1_read_req_schedule_q.push_back(entry);
                } else {
                    m_l1_write_req_schedule_q.push_back(entry);
                }
                ++it_addr;
                continue;
                /* SPIN */
            }

            /* line updated */
            mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] updated on address "
                      << l1_req->maddr() << " (TIMESTAMP: " << *l1_line_info->timestamp << " ) " << endl;

            if (l1_victim) {
                if (stats_enabled()) {
                    stats()->evict_at_l1();
                }

                if (l1_victim_info->checked_out && l1_victim_info->away) {
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] evicted an away checked-out line on address "
                              << l1_victim->start_maddr << " ( home : " << l1_victim_info->home << " ) " << endl;
                    remote_checkin = shared_ptr<coherenceMsg>(new coherenceMsg);
                    remote_checkin->sender = m_id;
                    remote_checkin->sent = false;
                    remote_checkin->type = REMOTE_CHECKIN;
                    remote_checkin->receiver = l1_victim_info->home;
                    if (l1_victim->data_dirty) {
                        remote_checkin->word_count = m_cfg.words_per_cache_line;
                        remote_checkin->data = l1_victim->data;
                    } else {
                        remote_checkin->word_count = 0;
                        remote_checkin->data = shared_array<uint32_t>();
                    }
                    remote_checkin->maddr = l1_victim->start_maddr;
                    remote_checkin->birthtime = system_time;
                    remote_checkin->coherence_info = l1_victim_info;

                    m_remote_rep_and_checkin_schedule_q.push_back(make_tuple(false, entry));

                    entry->status = _SEND_CHECKIN;
                    entry->substatus = _SEND_CHECKIN__VICTIM;

                    for (vector<tuple<shared_ptr<uint64_t>, maddr_t> >::iterator it = m_scheduled_checkin.begin();
                         it != m_scheduled_checkin.end(); ++it)
                    {
                        if (l1_victim->start_maddr == get<1>(*it)) {
                            m_scheduled_checkin.erase(it);
                            break;
                        }
                    }

                    ++it_addr;
                    continue;
                    /* TRANSITION */

                } else if (l1_victim->data_dirty || l1_victim->coherence_info_dirty) {
                    mh_assert(!l1_victim_info->away);
                    if (stats_enabled()) {
                        stats()->writeback_at_l1();
                    }

                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] evicted a dirty line on address "
                              << l1_victim->start_maddr << endl;

                    uint32_t word_count = (l1_victim->data_dirty)? m_cfg.words_per_cache_line : 0;
                    /* bug fix - if the line has been evicted from L2, it needs new data and coherence information */
                    //shared_array<uint32_t> data = (l1_victim->data_dirty)? l1_victim->data : shared_array<uint32_t>();
                    //shared_ptr<coherenceInfo> c_info = 
                    //    (l1_victim->coherence_info_dirty)? l1_victim_info : shared_ptr<coherenceInfo>();
                    shared_array<uint32_t> data = l1_victim->data;
                    shared_ptr<coherenceInfo> c_info = l1_victim_info;
                    l2_req = shared_ptr<cacheRequest>(new cacheRequest(l1_victim->start_maddr,
                                                                       CACHE_REQ_UPDATE,
                                                                       word_count, data, c_info));
                    l2_req->set_serialization_begin_time(system_time);
                    l2_req->set_unset_dirty_on_write(false);
                    l2_req->set_claim(true);
                    l2_req->set_evict(true);

                    if (l2_req->use_read_ports()) {
                        m_l2_read_req_schedule_q.push_back(entry);
                    } else {
                        m_l2_write_req_schedule_q.push_back(entry);
                    }
                    /* need to declare writebacking to prioritize (a separate data structure for simulation performance) */
                    m_l2_writeback_status[l1_victim->start_maddr] = entry;
                    
                    entry->status = _WRITEBACK_TO_L2;
                    entry->substatus = _WRITEBACK_TO_L2__FROM_L1;

                    ++it_addr;
                    continue;
                    /* TRANSITION */
                } else {
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] evicted a clean line on address "
                              << l1_victim->start_maddr << endl;
                }
            }

            /* _UPDATE_L1 - reached entries continue to the end of the loop and finish there */

        } else if (entry->status == _UPDATE_L2) {

            if (l2_req->status() == CACHE_REQ_NEW) {
                /* lost last arbitration */
                if (l2_req->use_read_ports()) {
                    m_l2_read_req_schedule_q.push_back(entry);
                } else {
                    m_l2_write_req_schedule_q.push_back(entry);
                }
                ++it_addr;
                continue;
                /* SPIN */
            } else if (l2_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
                /* SPIN */
            } 

            if (per_mem_instr_stats) {
                if (stats_enabled()) {
                    per_mem_instr_stats->get_tentative_data(T_IDX_L2)->add_l2_ops(system_time - l2_req->serialization_begin_time());
                    if (core_req) {
                        per_mem_instr_stats->add_local_l2_cost_for_update(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                    } else {
                        per_mem_instr_stats->add_remote_l2_cost_for_update(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                    }
                    per_mem_instr_stats->commit_tentative_data(T_IDX_L2);
                } else {
                    per_mem_instr_stats->clear_tentative_data();
                }
            }

            if (l2_req->status() == CACHE_REQ_MISS) {
                /* could not evict any line - must wait(retry) */
                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] could not update for cache full on address "
                          << l2_req->maddr() << endl;
                l2_req->set_serialization_begin_time(system_time);
                l2_req->reset();
                if (l2_req->use_read_ports()) {
                    m_l2_read_req_schedule_q.push_back(entry);
                } else {
                    m_l2_write_req_schedule_q.push_back(entry);
                }
                ++it_addr;
                continue;
                /* SPIN */
            }

            /* line updated */
            mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] updated on address "
                      << l2_req->maddr() << " (TIMESTAMP: " << *l2_line_info->timestamp << " ) " << endl;

            /* ready to make l1 fill request first */

            l1_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                               m_cfg.words_per_cache_line,
                                                               l2_line->data,
                                                               l2_line_info));
            l1_req->set_serialization_begin_time(system_time);
            l1_req->set_unset_dirty_on_write(true);
            l1_req->set_claim(true);
            l1_req->set_evict(true);

 
            if (l2_victim) {
                if (stats_enabled()) {
                    stats()->evict_at_l2();
                }
                if (l2_victim->data_dirty) {
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] evicted a dirty line on address "
                              << l2_victim->start_maddr << endl;
                    if (stats_enabled()) {
                        stats()->writeback_at_l2();
                    }

                    dramctrl_req = shared_ptr<dramctrlMsg>(new dramctrlMsg);
                    dramctrl_req->sender = m_id;
                    dramctrl_req->receiver = m_dramctrl_location;
                    dramctrl_req->maddr = l2_victim->start_maddr;
                    dramctrl_req->dram_req = shared_ptr<dramRequest>(new dramRequest(l2_victim->start_maddr,
                                                                                     DRAM_REQ_WRITE,
                                                                                     m_cfg.words_per_cache_line,
                                                                                     l2_victim->data));
                    dramctrl_req->sent = false;
                    dramctrl_req->birthtime = system_time;
                    dramctrl_req->per_mem_instr_stats = per_mem_instr_stats;

                    entry->status = _WRITEBACK_TO_DRAMCTRL;
                    entry->substatus = _WRITEBACK_TO_DRAMCTRL__FROM_UPDATE_L2;

                    m_dramctrl_req_schedule_q.push_back(make_tuple(false, entry)); /* mark as from local */
                    /* need to declare writebacking to prioritize (a separate data structure for simulation performance) */
                    m_dramctrl_writeback_status[l2_victim->start_maddr] = entry;

                    ++it_addr;
                    continue;
                    /* TRANSITION */
                } else {
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] evicted a clean line on address "
                              << l2_victim->start_maddr << endl;
                }
            }

            if (l1_req->use_read_ports()) {
                m_l1_read_req_schedule_q.push_back(entry);
            } else {
                m_l1_write_req_schedule_q.push_back(entry);
            }
            entry->status = _UPDATE_L1;

            ++it_addr;
            continue;
            /* TRANSITION */

        } else if (entry->status == _WRITEBACK_TO_L1) {
            if (l1_req->status() == CACHE_REQ_NEW) {
                /* lost last arbitration */
                if (l1_req->use_read_ports()) {
                    m_l1_read_req_schedule_q.push_back(entry);
                } else {
                    m_l1_write_req_schedule_q.push_back(entry);
                }
                ++it_addr;
                continue;
                /* SPIN */
            } else if (l1_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
                /* SPIN */
            } 

            if (l1_req->status() == CACHE_REQ_HIT) {
                if (stats_enabled()) {
                    stats()->checkin_hit_at_l1();
                }

                if (entry->using_read_exclusive_space) {
                    ++m_work_table_vacancy_read_exclusive;
                } else if (entry->using_send_checkin_exclusive_space) {
                    ++m_work_table_vacancy_send_checkin_exclusive;
                } else if (entry->using_receive_checkin_exclusive_space) {
                    ++m_work_table_vacancy_receive_checkin_exclusive;
                } else {
                    ++m_work_table_vacancy_shared;
                }

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] a checkin is hit at L1 on " << remote_checkin->maddr 
                          << endl;

                /* checking in must be write-through */

            } else {
                /* missed */
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] a checkin is missed at L1 on " << remote_checkin->maddr 
                          << endl;
            }

            l2_req = shared_ptr<cacheRequest>(new cacheRequest(remote_checkin->maddr, /* always be a start_maddr */
                                                               CACHE_REQ_UPDATE,
                                                               (remote_checkin->data)?m_cfg.words_per_cache_line:0,
                                                               (remote_checkin->data)?remote_checkin->data:shared_array<uint32_t>(),
                                                               remote_checkin->coherence_info));
            l2_req->set_serialization_begin_time(UINT64_MAX);
            l2_req->set_unset_dirty_on_write(false);
            l2_req->set_claim(false);
            l2_req->set_evict(false);

            entry->status = _WRITEBACK_TO_L2;
            entry->substatus = _WRITEBACK_TO_L2__FROM_CHECKIN;

            ++it_addr;
            continue;
            /* TRANSITION */
                
        } else if (entry->status == _WRITEBACK_TO_L2) {

            if (l2_req->status() == CACHE_REQ_NEW) {
                /* lost last arbitration */
                if (l2_req->use_read_ports()) {
                    m_l2_read_req_schedule_q.push_back(entry);
                } else {
                    m_l2_write_req_schedule_q.push_back(entry);
                }
                ++it_addr;
                continue;
                /* SPIN */
            } else if (l2_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
                /* SPIN */
            } 

            if (entry->substatus == _WRITEBACK_TO_L2__FROM_CHECKIN) {
                /* cannot evict before receiving a checkin*/
                if (l2_req->status() == CACHE_REQ_MISS) {
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] writeback to L2 for a checkin missed on " 
                              << remote_checkin->maddr << endl; 
                } else {
                    if (stats_enabled()) {
                        stats()->checkin_hit_at_l2();
                    }
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] a checkin is hit at L2 on " << remote_checkin->maddr 
                        << endl;
                }

                if (entry->using_read_exclusive_space) {
                    ++m_work_table_vacancy_read_exclusive;
                } else if (entry->using_send_checkin_exclusive_space) {
                    ++m_work_table_vacancy_send_checkin_exclusive;
                } else if (entry->using_receive_checkin_exclusive_space) {
                    ++m_work_table_vacancy_receive_checkin_exclusive;
                } else {
                    ++m_work_table_vacancy_shared;
                }
                m_work_table.erase(it_addr++);
                continue;
                /* FINISH */

            } else {
                if (per_mem_instr_stats) {
                    if (stats_enabled()) {
                        per_mem_instr_stats->get_tentative_data(T_IDX_L2)->add_l2_ops(system_time - l2_req->serialization_begin_time());
                        if (core_req) {
                            per_mem_instr_stats->add_local_l2_cost_for_writeback(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                        } else {
                            per_mem_instr_stats->add_remote_l2_cost_for_writeback(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                        }
                        per_mem_instr_stats->commit_tentative_data(T_IDX_L2);
                    } else {
                        per_mem_instr_stats->clear_tentative_data();
                    }
                }

                /* doing writeback to L2 from L1 */

                if (l2_req->status() == CACHE_REQ_MISS) {
                    
                    /* L2 is full and could not evict any line */
                    /* timestamp from L1 might be larger than L2. this line must get into the L2 */
                    l2_req->reset();
                    l2_req->set_serialization_begin_time(system_time);
                    if (l2_req->use_read_ports()) {
                        m_l2_read_req_schedule_q.push_back(entry);
                    } else {
                        m_l2_write_req_schedule_q.push_back(entry);
                    }
                    ++it_addr;
                    continue;
                    /* SPIN */

                } 
                
                /* writeback to l2 from l1 finished, so empty l2 writeback status */
                m_l2_writeback_status.erase(l1_victim->start_maddr);
                
                if (l2_victim) {
                    if (stats_enabled()) {
                        stats()->evict_at_l2();
                    }

                    if (l2_victim->data_dirty) {
                        mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] writeback evicted a dirty L2 line of "
                                  << l2_victim->start_maddr << endl;
                        dramctrl_req = shared_ptr<dramctrlMsg>(new dramctrlMsg);
                        dramctrl_req->sender = m_id;
                        dramctrl_req->receiver = m_dramctrl_location;
                        dramctrl_req->maddr = l2_victim->start_maddr;
                        dramctrl_req->dram_req = shared_ptr<dramRequest>(new dramRequest(l2_victim->start_maddr,
                                                                                         DRAM_REQ_WRITE,
                                                                                         m_cfg.words_per_cache_line,
                                                                                         l2_victim->data));
                        dramctrl_req->sent = false;
                        dramctrl_req->birthtime = system_time;
                        dramctrl_req->per_mem_instr_stats = per_mem_instr_stats;

                        entry->status = _WRITEBACK_TO_DRAMCTRL;
                        entry->substatus = _WRITEBACK_TO_DRAMCTRL__FROM_UPDATE_L1;

                        m_dramctrl_req_schedule_q.push_back(make_tuple(false, entry)); /* mark as from local */
                        /* need to declare writebacking to prioritize (a separate data structure for simulation performance) */
                        m_dramctrl_writeback_status[l2_victim->start_maddr] = entry;

                        ++it_addr;
                        continue;
                        /* TRANSITION */
                    }
                }

                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] written back for address "
                          << l1_victim->start_maddr << endl;
                /* reached entries continue to the end of the loop and finish there */

            }

        } else if (entry->status == _WRITEBACK_TO_DRAMCTRL) {
            if (!dramctrl_req->sent) {
                m_dramctrl_req_schedule_q.push_back(make_tuple(false, entry)); /* mark as from local */
                /* need to declare writebacking to prioritize (a separate data structure for simulation performance) */
                m_dramctrl_writeback_status[l2_victim->start_maddr] =  entry;
                ++it_addr;
                continue;
                /* SPIN */
            }

            if (stats_enabled() && per_mem_instr_stats) {
                per_mem_instr_stats->add_dramctrl_req_nas(system_time - dramctrl_req->birthtime);
            }

            if (entry->substatus == _WRITEBACK_TO_DRAMCTRL__FROM_UPDATE_L1) {

                mh_log(4) << "[DRAMCTRL " << m_id << " @ " << system_time << " ] written back sent for address "
                          << l2_victim->start_maddr << endl;
                /* reached entries continue to the end of the loop and finish there */

            } else {
                /* _WRITEBACK_TO_DRAMCTRL__FROM_UPDATE_L2 */
                mh_log(4) << "[DRAMCTRL " << m_id << " @ " << system_time << " ] written back sent for address "
                          << l2_victim->start_maddr << endl;
                if (l1_req->use_read_ports()) {
                    m_l1_read_req_schedule_q.push_back(entry);
                } else {
                    m_l1_write_req_schedule_q.push_back(entry);
                }
                entry->status = _UPDATE_L1;

                ++it_addr;
                continue;
                /* TRANSITION */
            }

            /* _WRITEBACK_TO_DRAMCTRL */

        } else if (entry->status == _SEND_CHECKIN) {
            if (!remote_checkin) {
                /* yet to invalidate L1 */
                if (l1_req->status() == CACHE_REQ_NEW) {
                    /* lost last arbitration */
                    if (l1_req->use_read_ports()) {
                        m_l1_read_req_schedule_q.push_back(entry);
                    } else {
                        m_l1_write_req_schedule_q.push_back(entry);
                    }
                    ++it_addr;
                    continue;
                    /* SPIN */
                } else if (l1_req->status() == CACHE_REQ_WAIT) {
                    ++it_addr;
                    continue;
                    /* SPIN */
                } else if (l1_req->status() == CACHE_REQ_MISS) {
                    /* already invalidated and a checkin message must have been sent */

                    if (entry->using_read_exclusive_space) {
                        ++m_work_table_vacancy_read_exclusive;
                    } else if (entry->using_send_checkin_exclusive_space) {
                        ++m_work_table_vacancy_send_checkin_exclusive;
                    } else if (entry->using_receive_checkin_exclusive_space) {
                        ++m_work_table_vacancy_receive_checkin_exclusive;
                    } else {
                        ++m_work_table_vacancy_shared;
                    }
                    m_work_table.erase(it_addr++);
                    continue;
                    /* FINISHED */
                } else {
                    /* hit - a line invalidated */
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] a checked-out line invalidated on address "
                              << l1_line->start_maddr << endl;
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] sending a checkin to " << l1_line_info->home << endl;
                    remote_checkin = shared_ptr<coherenceMsg>(new coherenceMsg);
                    remote_checkin->sender = m_id;
                    remote_checkin->sent = false;
                    remote_checkin->type = REMOTE_CHECKIN;
                    remote_checkin->receiver = l1_line_info->home;
                    if (l1_line->data_dirty) {
                        remote_checkin->word_count = m_cfg.words_per_cache_line;
                        remote_checkin->data = l1_line->data;
                    } else {
                        remote_checkin->word_count = 0;
                        remote_checkin->data = shared_array<uint32_t>();
                    }
                    remote_checkin->maddr = start_maddr;
                    remote_checkin->birthtime = UINT64_MAX;
                    remote_checkin->coherence_info = l1_line_info;

                    m_remote_rep_and_checkin_schedule_q.push_back(make_tuple(false,entry));
                    ++it_addr;
                    continue;
                    /* SPIN */

                }
            } else if (remote_checkin->sent) {
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] a checkin message sent on address "
                          << remote_checkin->maddr << endl;
                if (entry->substatus == _SEND_CHECKIN__VOLUNTARY) {
                    if (stats_enabled()) {
                        stats()->remote_checkin_expired_sent();
                    }
                    if (entry->using_read_exclusive_space) {
                        ++m_work_table_vacancy_read_exclusive;
                    } else if (entry->using_send_checkin_exclusive_space) {
                        ++m_work_table_vacancy_send_checkin_exclusive;
                    } else if (entry->using_receive_checkin_exclusive_space) {
                        ++m_work_table_vacancy_receive_checkin_exclusive;
                    } else {
                        ++m_work_table_vacancy_shared;
                    }
                    m_work_table.erase(it_addr++);
                    continue;
                    /* FINISHED */
                } else if (entry->substatus == _SEND_CHECKIN__UNCACHEABLE) {
                    if (stats_enabled()) {
                        stats()->remote_checkin_uncacheable_sent();
                        if (per_mem_instr_stats) {
                            per_mem_instr_stats->add_remote_checkin_nas(system_time - remote_checkin->birthtime);
                            stats()->commit_per_mem_instr_stats(per_mem_instr_stats);
                        }
                        if (core_req->is_read()) {
                            stats()->did_finish_read();
                        } else {
                            stats()->did_finish_write();
                        }
                    }

                    set_req_status(core_req, REQ_DONE);
                    ++m_available_core_ports;

                    if (entry->using_read_exclusive_space) {
                        ++m_work_table_vacancy_read_exclusive;
                    } else if (entry->using_send_checkin_exclusive_space) {
                        ++m_work_table_vacancy_send_checkin_exclusive;
                    } else if (entry->using_receive_checkin_exclusive_space) {
                        ++m_work_table_vacancy_receive_checkin_exclusive;
                    } else {
                        ++m_work_table_vacancy_shared;
                    }

                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] finish serving address "
                        << core_req->maddr() << endl;

                    m_work_table.erase(it_addr++);
                    continue;
                    /* FINISH */

                } else {
                    /* __VICTIM */
                    if (stats_enabled()) {
                        stats()->remote_checkin_evicted_sent();
                        if (per_mem_instr_stats) {
                            per_mem_instr_stats->add_remote_checkin_nas(system_time - remote_checkin->birthtime);
                        }
                    }

                    /* reached entries continue to the end of the loop and finish there */

                }
            } else {
                /* yet to send */
                m_remote_rep_and_checkin_schedule_q.push_back(make_tuple(false, entry));
                ++it_addr;
                continue;
                /* SPIN */
            }

        } else if (entry->status == _SEND_DRAMCTRL_REQ) {
            if (!dramctrl_req->sent) {
                m_dramctrl_req_schedule_q.push_back(make_tuple(false, entry));
                ++it_addr;
                continue;
                /* SPIN */
            }

            mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] sent a DRAM request for "
                      << dramctrl_req->dram_req->maddr() << " to " << dramctrl_req->receiver << endl;
            
            entry->status = _WAIT_DRAMCTRL_REP;
            ++it_addr;
            continue;
            /* TRANSITION */

        } else if (entry->status == _WAIT_DRAMCTRL_REP) {

            if (!dramctrl_rep) {
                ++it_addr;
                continue;
                /* SPIN */
            }

            if (stats_enabled() && per_mem_instr_stats) {
                per_mem_instr_stats->add_dramctrl_rep_nas(system_time - dramctrl_rep->birthtime);
            }

            shared_ptr<coherenceInfo> new_coherence_info(new coherenceInfo);
            mh_assert(m_handler_for_dramctrl_rep);
            bool for_read = (core_req && core_req->is_read()) || (remote_req && remote_req->type==REMOTE_READ_REQ);
            uint32_t requester = (core_req)? m_id : remote_req->sender;

            mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] received a DRAM reply for "
                      << dramctrl_rep->dram_req->maddr() << " (requester : " << requester << " , ";
            if (for_read) {
                mh_log(4) << " read ) " << endl;
            } else {
                mh_log(4) << " write ) " << endl;
            }

            shared_ptr<auxInfoForCoherence> req_info(
                new auxInfoForCoherence(m_id, (core_req)? m_id : remote_req->sender, for_read, m_cfg));
            (*m_handler_for_dramctrl_rep)(new_coherence_info, req_info, system_time);

            if (l2_req->request_type() == CACHE_REQ_WRITE) {
                uint32_t word_offset = (l2_req->maddr().address / 4 )  % m_cfg.words_per_cache_line;
                for (uint32_t i = 0; i < l2_req->word_count(); ++i) {
                    dramctrl_rep->dram_req->read()[i + word_offset] = l2_req->data_to_write()[i];
                }
            }

            l2_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                               m_cfg.words_per_cache_line, 
                                                               dramctrl_rep->dram_req->read(),
                                                               new_coherence_info));
            l2_req->set_serialization_begin_time(system_time);
            l2_req->set_unset_dirty_on_write(for_read);
            l2_req->set_claim(true);
            l2_req->set_evict(true);

            if (l2_req->use_read_ports()) {
                m_l2_read_req_schedule_q.push_back(entry);
            } else {
                m_l2_write_req_schedule_q.push_back(entry);
            }

            entry->status = _UPDATE_L2;

            ++it_addr;
            continue;
            /* TRANSITION */
        }

        /* entries reached here finishes */
        if (core_req) {
            if (stats_enabled()) {
                if (per_mem_instr_stats) {
                    stats()->commit_per_mem_instr_stats(per_mem_instr_stats);
                }
                if (core_req->is_read()) {
                    stats()->did_finish_read();
                } else {
                    stats()->did_finish_write();
                }
            }
            shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
            uint32_t word_offset = (core_req->maddr().address / 4 ) % m_cfg.words_per_cache_line;
            for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                ret[i] = l1_line->data[i + word_offset];
            }
            set_req_data(core_req, ret);
            set_req_status(core_req, REQ_DONE);
            ++m_available_core_ports;

            if (entry->using_read_exclusive_space) {
                ++m_work_table_vacancy_read_exclusive;
            } else if (entry->using_send_checkin_exclusive_space) {
                ++m_work_table_vacancy_send_checkin_exclusive;
            } else if (entry->using_receive_checkin_exclusive_space) {
                ++m_work_table_vacancy_receive_checkin_exclusive;
            } else {
                ++m_work_table_vacancy_shared;
            }

            mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] finish serving address "
                << core_req->maddr() << endl;

            m_work_table.erase(it_addr++);
            continue;
            /* FINISH */
        } else {
            mh_assert(remote_req);
            remote_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
            remote_rep->sender = m_id;
            remote_rep->sent = false;
            remote_rep->type = REMOTE_REP;
            remote_rep->receiver = remote_req->sender;
            if (remote_req->type == REMOTE_READ_REQ) {
                remote_rep->word_count = remote_req->word_count;
            } else {
                remote_rep->word_count = 0;
            }
            remote_rep->maddr = remote_req->maddr;
            remote_rep->data = l1_line->data;
            remote_rep->birthtime = system_time;
            remote_rep->coherence_info = l1_line_info; /* l1 line info is not used in this core later, so need not make a copy */

            entry->status = _SEND_REMOTE_REP;
            entry->substatus = _SEND_REMOTE_REP__CACHE;

            m_remote_rep_and_checkin_schedule_q.push_back(make_tuple(true, entry));

            ++it_addr;

            continue;
            /* TRANSITION */
        }

    }

}

void sharedSharedLCC::update_dramctrl_work_table() {

    for (dramctrlTable::iterator it_addr = m_dramctrl_work_table.begin(); it_addr != m_dramctrl_work_table.end(); ) {
        shared_ptr<dramctrlTableEntry>& entry = it_addr->second;
        shared_ptr<dramctrlMsg>& dramctrl_req = entry->dramctrl_req;
        shared_ptr<dramctrlMsg>& dramctrl_rep = entry->dramctrl_rep;
        shared_ptr<sharedSharedLCCStatsPerMemInstr>& per_mem_instr_stats = entry->per_mem_instr_stats;

        /* only reads are in the table */
        if (dramctrl_req->dram_req->status() == DRAM_REQ_DONE) {

            if (!dramctrl_rep) {
                if (stats_enabled() && per_mem_instr_stats) {
                    per_mem_instr_stats->add_dram_ops(system_time - entry->operation_begin_time);
                }
                dramctrl_rep = shared_ptr<dramctrlMsg>(new dramctrlMsg);
                dramctrl_rep->sender = m_id;
                dramctrl_rep->maddr = dramctrl_req->maddr;
                dramctrl_rep->dram_req = dramctrl_req->dram_req;
                dramctrl_rep->sent = false;
                dramctrl_rep->birthtime = system_time;
                dramctrl_rep->per_mem_instr_stats = per_mem_instr_stats;
            }

            if (!dramctrl_rep->sent) {
                m_dramctrl_rep_schedule_q.push_back(entry);
                ++it_addr;
                continue;
            } else {
                m_dramctrl_work_table.erase(it_addr++);
                continue;
            }
        } else {
            ++it_addr;
            continue;;
        }

    }

}

void sharedSharedLCC::accept_incoming_messages() {

    if (m_core_receive_queues[MSG_REMOTE_REQ]->size()) {
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_REMOTE_REQ]->front();
        shared_ptr<coherenceMsg> data_msg = static_pointer_cast<coherenceMsg>(msg->content);
        m_new_work_table_entry_schedule_q.push_back(make_tuple(FROM_CHANNEL_REMOTE_REQ, data_msg));
    }

    if (m_core_receive_queues[MSG_REMOTE_WRITE_REQ]->size()) {
        /* only when using a separate VCset for write requests */
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_REMOTE_WRITE_REQ]->front();
        shared_ptr<coherenceMsg> data_msg = static_pointer_cast<coherenceMsg>(msg->content);
        m_new_work_table_entry_schedule_q.push_back(make_tuple(FROM_CHANNEL_REMOTE_WRITE_REQ, data_msg));
    }

    if (m_core_receive_queues[MSG_REMOTE_REP]->size()) {
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_REMOTE_REP]->front();
        shared_ptr<coherenceMsg> data_msg = static_pointer_cast<coherenceMsg>(msg->content);
        maddr_t msg_start_maddr = get_start_maddr_in_line(data_msg->maddr);
        if (data_msg->type == REMOTE_REP) {
            mh_assert(m_work_table.count(msg_start_maddr) && 
                      m_work_table[msg_start_maddr]->status == _WAIT_REMOTE_REP);
            m_work_table[msg_start_maddr]->remote_rep = data_msg;
            m_core_receive_queues[MSG_REMOTE_REP]->pop();
        } else {
            /* checkins */
            if (m_work_table.count(msg_start_maddr)) {
                /* the current entry must be stuck (or will be) in the WAIT_CHECKIN status and take this message */
                m_work_table[msg_start_maddr]->remote_checkin = data_msg;
                mh_log(4) << "[Mem " << m_id << " @ " << system_time 
                          << " ] A remote checkin from " << data_msg->sender << " on " 
                          << data_msg->maddr << " is received. " << endl;
                m_core_receive_queues[MSG_REMOTE_REP]->pop();
            } else {
                m_new_work_table_entry_schedule_q.push_back(make_tuple(FROM_CHANNEL_REMOTE_REP, data_msg));
            }
        }
    }

    if (m_core_receive_queues[MSG_DRAMCTRL_REQ]->size()) {
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_DRAMCTRL_REQ]->front();
        shared_ptr<dramctrlMsg> dram_msg = static_pointer_cast<dramctrlMsg>(msg->content);
        m_dramctrl_req_schedule_q.push_back(make_tuple(true, dram_msg));
    }

    if (m_core_receive_queues[MSG_DRAMCTRL_REP]->size()) {
        /* note: no replies for DRAM writes */
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_DRAMCTRL_REP]->front();
        shared_ptr<dramctrlMsg> dramctrl_msg = static_pointer_cast<dramctrlMsg>(msg->content);
        maddr_t start_maddr = dramctrl_msg->dram_req->maddr(); /* always access by a cache line */
        mh_assert(m_work_table.count(start_maddr) > 0 &&
                  m_work_table[dramctrl_msg->maddr]->status == _WAIT_DRAMCTRL_REP);
        m_work_table[start_maddr]->dramctrl_rep = dramctrl_msg;
        m_core_receive_queues[MSG_DRAMCTRL_REP]->pop();
    }

}

void sharedSharedLCC::schedule_requests() {

    static boost::function<int(int)> rr_fn = bind(&random_gen::random_range, ran, _1);

    /**********************************/
    /* scheduling for sending checkin */
    /**********************************/
    /* checkins have highest priority in work table arbitration */
    for (vector<tuple<shared_ptr<uint64_t>, maddr_t> >::iterator it = m_scheduled_checkin.begin(); it != m_scheduled_checkin.end();) {
        uint64_t scheduled_time = *(get<0>(*it));
        maddr_t start_maddr = get<1>(*it);
        if (system_time < scheduled_time) {
            ++it;
            continue;
        }
        bool no_space = (m_work_table_vacancy_shared == 0) && (m_work_table_vacancy_send_checkin_exclusive == 0);
        if (m_work_table.count(start_maddr) || no_space) {
            ++it;
            continue;
        }
        bool use_exclusive = m_work_table_vacancy_send_checkin_exclusive > 0;

        shared_ptr<cacheRequest> l1_req (new cacheRequest(start_maddr, CACHE_REQ_INVALIDATE, m_cfg.words_per_cache_line));
        l1_req->set_serialization_begin_time(UINT64_MAX);
        l1_req->set_unset_dirty_on_write(false);
        l1_req->set_claim(false);
        l1_req->set_evict(false);

        shared_ptr<tableEntry> new_entry(new tableEntry);
        new_entry->core_req = shared_ptr<memoryRequest>();
        new_entry->cat_req = shared_ptr<catRequest>();
        new_entry->l1_req = l1_req; /* valid */
        new_entry->l2_req = shared_ptr<cacheRequest>();
        new_entry->remote_req = shared_ptr<coherenceMsg>();
        new_entry->remote_rep = shared_ptr<coherenceMsg>();
        new_entry->bypass_remote_req = shared_ptr<coherenceMsg>();
        new_entry->bypass_core_req = shared_ptr<memoryRequest>();
        new_entry->remote_checkin = shared_ptr<coherenceMsg>();
        new_entry->dramctrl_req = shared_ptr<dramctrlMsg>();
        new_entry->dramctrl_rep = shared_ptr<dramctrlMsg>();
        new_entry->per_mem_instr_stats = shared_ptr<sharedSharedLCCStatsPerMemInstr>();

        new_entry->can_bypass_read_req = false;

        new_entry->status = _SEND_CHECKIN;
        new_entry->substatus = _SEND_CHECKIN__VOLUNTARY;

        if (l1_req->use_read_ports()) {
            m_l1_read_req_schedule_q.push_back(new_entry);
        } else {
            m_l1_write_req_schedule_q.push_back(new_entry);
        }

        m_work_table[start_maddr] = new_entry;

        mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] created an entry to send a checkin on " 
                  << start_maddr << endl;
        
        new_entry->using_read_exclusive_space = false;
        new_entry->using_receive_checkin_exclusive_space = false;
        if (use_exclusive) {
            --m_work_table_vacancy_send_checkin_exclusive;
            new_entry->using_send_checkin_exclusive_space = true;
        } else {
            --m_work_table_vacancy_shared;
            new_entry->using_send_checkin_exclusive_space = false;
        }
        m_scheduled_checkin.erase(it);
    }

    /*****************************/
    /* scheduling for core ports */
    /*****************************/

    /* schedule for core ports first */
    random_shuffle(m_core_port_schedule_q.begin(), m_core_port_schedule_q.end(), rr_fn);
    uint32_t requested = 0;
    while (m_core_port_schedule_q.size()) {
        shared_ptr<memoryRequest> req = m_core_port_schedule_q.front();
        if (requested < m_available_core_ports) {
            m_new_work_table_entry_schedule_q.push_back(make_tuple(FROM_LOCAL_CORE, req));
            ++requested;
        } else {
            set_req_status(req, REQ_RETRY);
        }
        m_core_port_schedule_q.erase(m_core_port_schedule_q.begin());
    }

    /* schedule for work table space */
    random_shuffle(m_new_work_table_entry_schedule_q.begin(), m_new_work_table_entry_schedule_q.end(), rr_fn);
    while (m_new_work_table_entry_schedule_q.size()) {
        work_table_entry_src_t req_from = m_new_work_table_entry_schedule_q.front().get<0>();
        if (req_from == FROM_CHANNEL_REMOTE_REQ || req_from == FROM_CHANNEL_REMOTE_WRITE_REQ) {
            shared_ptr<coherenceMsg> remote_req = static_pointer_cast<coherenceMsg>(m_new_work_table_entry_schedule_q.front().get<1>());
            maddr_t start_maddr = get_start_maddr_in_line(remote_req->maddr);
            shared_ptr<sharedSharedLCCStatsPerMemInstr> per_mem_instr_stats = remote_req->per_mem_instr_stats;

            bool is_read = (remote_req->type == REMOTE_READ_REQ);
            bool has_space = ( m_work_table_vacancy_shared > 0) || (is_read && m_work_table_vacancy_read_exclusive);
            bool use_exclusive = is_read && m_work_table_vacancy_read_exclusive > 0;
            
            if (is_read && m_work_table.count(start_maddr) 
                && m_work_table[start_maddr]->can_bypass_read_req && !m_work_table[start_maddr]->bypass_remote_req) 
            {
                m_work_table[start_maddr]->bypass_remote_req_begin_time = system_time;
                m_work_table[start_maddr]->bypass_remote_req = remote_req;
                mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] received a bypassable read request on " 
                          << remote_req->maddr << " for core " << remote_req->sender << " from " << req_from << endl;
                mh_assert(req_from == FROM_CHANNEL_REMOTE_REQ);
                m_core_receive_queues[MSG_REMOTE_REQ]->pop();
                m_new_work_table_entry_schedule_q.erase(m_new_work_table_entry_schedule_q.begin());
                if (stats_enabled()) {
                    stats()->new_read_instr_at_l1();
                }
                continue;
            } else if (m_work_table.count(start_maddr) || !has_space) {
                m_new_work_table_entry_schedule_q.erase(m_new_work_table_entry_schedule_q.begin());
                continue;
            } 

            shared_ptr<cacheRequest> l1_req (new cacheRequest(remote_req->maddr,
                                                              (is_read)? CACHE_REQ_READ : CACHE_REQ_WRITE,
                                                              remote_req->word_count,
                                                              (is_read)? shared_array<uint32_t>() : remote_req->data));
            l1_req->set_serialization_begin_time(system_time);
            l1_req->set_unset_dirty_on_write(false);
            l1_req->set_claim(false);
            l1_req->set_evict(false);
            l1_req->set_aux_info_for_coherence(
                shared_ptr<auxInfoForCoherence>(new auxInfoForCoherence(m_id, remote_req->sender, is_read, m_cfg))
            );

            shared_ptr<tableEntry> new_entry(new tableEntry);
            new_entry->core_req = shared_ptr<memoryRequest>();
            new_entry->cat_req = shared_ptr<catRequest>();
            new_entry->l1_req = l1_req; /* valid */
            new_entry->l2_req = shared_ptr<cacheRequest>();
            new_entry->remote_req = remote_req; /* valid */
            new_entry->remote_rep = shared_ptr<coherenceMsg>();
            new_entry->bypass_remote_req = shared_ptr<coherenceMsg>();
            new_entry->bypass_core_req = shared_ptr<memoryRequest>();
            new_entry->remote_checkin = shared_ptr<coherenceMsg>();
            new_entry->dramctrl_req = shared_ptr<dramctrlMsg>();
            new_entry->dramctrl_rep = shared_ptr<dramctrlMsg>();
            new_entry->per_mem_instr_stats = per_mem_instr_stats; /* valid */

            new_entry->can_bypass_read_req = false;
         
            new_entry->status = _L1_FOR_REMOTE;

            if (l1_req->use_read_ports()) {
                m_l1_read_req_schedule_q.push_back(new_entry);
            } else {
                m_l1_write_req_schedule_q.push_back(new_entry);
            }

            m_work_table[start_maddr] = new_entry;

            new_entry->using_send_checkin_exclusive_space = false;
            new_entry->using_receive_checkin_exclusive_space = false;
            if (use_exclusive) {
                --m_work_table_vacancy_read_exclusive;
                new_entry->using_read_exclusive_space = true;
            } else {
                --m_work_table_vacancy_shared;
                new_entry->using_read_exclusive_space = false;
            }

            /* pop from the network receive queue */
            if (stats_enabled()) {
                if (is_read) {
                    stats()->new_read_instr_at_l1();
                } else {
                    stats()->new_write_instr_at_l1();
                }
                if (new_entry->per_mem_instr_stats) {
                    new_entry->per_mem_instr_stats->add_remote_req_nas(system_time - remote_req->birthtime);
                }
            }
            if (req_from == FROM_CHANNEL_REMOTE_REQ) {
                m_core_receive_queues[MSG_REMOTE_REQ]->pop();
            } else {
                m_core_receive_queues[MSG_REMOTE_WRITE_REQ]->pop();
            }

            m_new_work_table_entry_schedule_q.erase(m_new_work_table_entry_schedule_q.begin());

            mh_log(4) << "[Mem " << m_id << " @ " << system_time 
                      << " ] A remote request from " << remote_req->sender << " on " 
                      << remote_req->maddr << " got into the table " << endl;

        } else if (req_from == FROM_CHANNEL_REMOTE_REP) {
            /* a checkin */
            shared_ptr<coherenceMsg> remote_checkin = 
                static_pointer_cast<coherenceMsg>(m_new_work_table_entry_schedule_q.front().get<1>());
            maddr_t start_maddr = get_start_maddr_in_line(remote_checkin->maddr);

            bool no_space = (m_work_table_vacancy_shared == 0) && (m_work_table_vacancy_receive_checkin_exclusive == 0);
            bool use_exclusive = m_work_table_vacancy_receive_checkin_exclusive > 0;
            
            if (m_work_table.count(start_maddr)) {
                /* another request first created an entry for this line */
                /* the entry will be stucj in the WAIT_CHECKIN status and take this message */
                m_work_table[start_maddr]->remote_checkin = remote_checkin;
                m_core_receive_queues[MSG_REMOTE_REP]->pop();
                m_new_work_table_entry_schedule_q.erase(m_new_work_table_entry_schedule_q.begin());

                mh_log(4) << "[Mem " << m_id << " @ " << system_time 
                          << " ] A remote checkin from " << remote_checkin->sender << " on " 
                          << remote_checkin->maddr << " is received. " << endl;

                continue;
            } else if (no_space) {
                m_new_work_table_entry_schedule_q.erase(m_new_work_table_entry_schedule_q.begin());
                continue;
            } else {

                mh_assert(m_handler_for_remote_checkin);
                (*m_handler_for_remote_checkin)(remote_checkin->coherence_info, shared_ptr<auxInfoForCoherence>(), system_time);
                /* null ptr indicates there is no waiting request for this remote checkin */

                shared_ptr<cacheRequest> l1_req (new cacheRequest(remote_checkin->maddr, /* always be a start_maddr */
                                                                  CACHE_REQ_UPDATE,
                                                                  (remote_checkin->data)?m_cfg.words_per_cache_line : 0,
                                                                  (remote_checkin->data)?remote_checkin->data:shared_array<uint32_t>(),
                                                                  remote_checkin->coherence_info));
                                                                  
                l1_req->set_serialization_begin_time(UINT64_MAX); /* don't record per instruction stats */
                l1_req->set_unset_dirty_on_write(false);
                l1_req->set_claim(false);
                l1_req->set_evict(false);

                shared_ptr<tableEntry> new_entry(new tableEntry);
                new_entry->core_req = shared_ptr<memoryRequest>();
                new_entry->cat_req = shared_ptr<catRequest>();
                new_entry->l1_req = l1_req; /* valid */
                new_entry->l2_req = shared_ptr<cacheRequest>();
                new_entry->remote_req = shared_ptr<coherenceMsg>(); 
                new_entry->remote_req = shared_ptr<coherenceMsg>();
                new_entry->bypass_remote_req = shared_ptr<coherenceMsg>();
                new_entry->bypass_core_req = shared_ptr<memoryRequest>();
                new_entry->remote_rep = shared_ptr<coherenceMsg>();
                new_entry->remote_checkin = remote_checkin; /* valid */
                new_entry->dramctrl_req = shared_ptr<dramctrlMsg>();
                new_entry->dramctrl_rep = shared_ptr<dramctrlMsg>();
                new_entry->per_mem_instr_stats = shared_ptr<sharedSharedLCCStatsPerMemInstr>();

                new_entry->can_bypass_read_req = false;

                new_entry->status = _WRITEBACK_TO_L1;

                if (l1_req->use_read_ports()) {
                    m_l1_read_req_schedule_q.push_back(new_entry);
                } else {
                    m_l1_write_req_schedule_q.push_back(new_entry);
                }

                m_work_table[start_maddr] = new_entry;

                new_entry->using_send_checkin_exclusive_space = false;
                new_entry->using_read_exclusive_space = false;
                if (use_exclusive) {
                    --m_work_table_vacancy_receive_checkin_exclusive;
                    new_entry->using_receive_checkin_exclusive_space = true;
                } else {
                    --m_work_table_vacancy_shared;
                    new_entry->using_receive_checkin_exclusive_space = false;
                }

                m_core_receive_queues[MSG_REMOTE_REP]->pop();

                m_new_work_table_entry_schedule_q.erase(m_new_work_table_entry_schedule_q.begin());

                mh_log(4) << "[Mem " << m_id << " @ " << system_time 
                          << " ] A remote checkin from " << remote_checkin->sender << " on " 
                          << remote_checkin->maddr << " created a new entry into the table " << endl;

                continue;
            }

        } else {
            shared_ptr<memoryRequest> core_req = static_pointer_cast<memoryRequest>(m_new_work_table_entry_schedule_q.front().get<1>());
            maddr_t start_maddr = get_start_maddr_in_line(core_req->maddr());
            shared_ptr<sharedSharedLCCStatsPerMemInstr> per_mem_instr_stats = 
                (core_req->per_mem_instr_runtime_info())? 
                    static_pointer_cast<sharedSharedLCCStatsPerMemInstr>(*(core_req->per_mem_instr_runtime_info()))
                :
                    shared_ptr<sharedSharedLCCStatsPerMemInstr>();

            bool has_space = (m_work_table_vacancy_shared > 0) || (core_req->is_read() && m_work_table_vacancy_read_exclusive > 0);
            bool use_exclusive = core_req->is_read() && m_work_table_vacancy_read_exclusive > 0;
            
            if (core_req->is_read() && m_work_table.count(start_maddr) 
                && m_work_table[start_maddr]->can_bypass_read_req && !m_work_table[start_maddr]->bypass_core_req) 
            {
                if (stats_enabled()) {
                    if (per_mem_instr_stats) {
                        per_mem_instr_stats->add_mem_srz(system_time - per_mem_instr_stats->serialization_begin_time_at_current_core());
                    }
                    stats()->new_read_instr_at_l1();
                }
                m_work_table[start_maddr]->bypass_core_req = core_req;
                m_work_table[start_maddr]->bypass_core_req_begin_time = system_time;
                --m_available_core_ports;
                m_new_work_table_entry_schedule_q.erase(m_new_work_table_entry_schedule_q.begin());
                continue;

            } else if (m_work_table.count(start_maddr) || !has_space) {
                set_req_status(core_req, REQ_RETRY);
                m_new_work_table_entry_schedule_q.erase(m_new_work_table_entry_schedule_q.begin());
                continue;
            }

            if (stats_enabled()) {
                if (per_mem_instr_stats) {
                    per_mem_instr_stats->add_mem_srz(system_time - per_mem_instr_stats->serialization_begin_time_at_current_core());
                }
                if (core_req->is_read()) {
                    stats()->new_read_instr_at_l1();
                } else {
                    stats()->new_write_instr_at_l1();
                }
            }

            shared_ptr<catRequest> cat_req(new catRequest(core_req->maddr(), m_id));
            cat_req->set_serialization_begin_time(system_time);

            shared_ptr<cacheRequest> l1_req(new cacheRequest(core_req->maddr(),
                                                             core_req->is_read()? CACHE_REQ_READ : CACHE_REQ_WRITE,
                                                             core_req->word_count(),
                                                             core_req->is_read()? shared_array<uint32_t>() : core_req->data()));
            l1_req->set_serialization_begin_time(system_time);
            l1_req->set_unset_dirty_on_write(false);
            l1_req->set_claim(false);
            l1_req->set_evict(false);
            l1_req->set_aux_info_for_coherence(
                shared_ptr<auxInfoForCoherence>(new auxInfoForCoherence(m_id, m_id, core_req->is_read(), m_cfg))
            );

            shared_ptr<tableEntry> new_entry(new tableEntry);
            new_entry->core_req = core_req; /* valid */
            new_entry->cat_req = cat_req; /* valid */
            new_entry->l1_req = l1_req; /* valid */
            new_entry->l2_req = shared_ptr<cacheRequest>();
            new_entry->remote_req = shared_ptr<coherenceMsg>();
            new_entry->remote_rep = shared_ptr<coherenceMsg>();
            new_entry->bypass_remote_req = shared_ptr<coherenceMsg>();
            new_entry->bypass_core_req = shared_ptr<memoryRequest>();
            new_entry->remote_checkin = shared_ptr<coherenceMsg>();
            new_entry->dramctrl_req = shared_ptr<dramctrlMsg>();
            new_entry->dramctrl_rep = shared_ptr<dramctrlMsg>();
            new_entry->per_mem_instr_stats = per_mem_instr_stats; /* valid */

            new_entry->can_bypass_read_req = false;

            new_entry->status = _CAT_AND_L1_FOR_LOCAL;

            m_cat_req_schedule_q.push_back(new_entry);
            if (l1_req->use_read_ports()) {
                m_l1_read_req_schedule_q.push_back(new_entry);
            } else {
                m_l1_write_req_schedule_q.push_back(new_entry);
            }

            m_work_table[start_maddr] = new_entry;

            new_entry->using_receive_checkin_exclusive_space = false;
            new_entry->using_send_checkin_exclusive_space = false;
            if (use_exclusive) {
                --m_work_table_vacancy_read_exclusive;
                new_entry->using_read_exclusive_space = true;
            } else {
                --m_work_table_vacancy_shared;
                new_entry->using_read_exclusive_space = false;
            }
            --m_available_core_ports;

            m_new_work_table_entry_schedule_q.erase(m_new_work_table_entry_schedule_q.begin());

            mh_log(4) << "[Mem " << m_id << " @ " << system_time 
                      << " ] A core request on " << core_req->maddr() << " got into the table " << endl;
        }
    }
    m_new_work_table_entry_schedule_q.clear();

    /************************************/
    /* scheduling for dramctrl requests */
    /************************************/
    
    random_shuffle(m_dramctrl_req_schedule_q.begin(), m_dramctrl_req_schedule_q.end(), rr_fn);
    while (m_dramctrl_req_schedule_q.size()) {
        bool is_remote = m_dramctrl_req_schedule_q.front().get<0>();
        shared_ptr<tableEntry> entry = shared_ptr<tableEntry>();
        shared_ptr<dramctrlMsg> dramctrl_msg = shared_ptr<dramctrlMsg>();
        if (is_remote) {
            dramctrl_msg = static_pointer_cast<dramctrlMsg>(m_dramctrl_req_schedule_q.front().get<1>());
        } else {
            entry = static_pointer_cast<tableEntry>(m_dramctrl_req_schedule_q.front().get<1>());
            dramctrl_msg = entry->dramctrl_req;
            if (m_dramctrl_writeback_status.count(entry->dramctrl_req->maddr) && 
                m_dramctrl_writeback_status[entry->dramctrl_req->maddr] != entry) 
            {
                mh_log(4) << "[DRAMCTRL " << m_id << " @ " << system_time 
                          << " ] A local DRAM read request on " << dramctrl_msg->maddr << " blocked by a writeback " << endl;
                
                m_dramctrl_req_schedule_q.erase(m_dramctrl_req_schedule_q.begin());
                continue;
            }
        }

        if (m_dramctrl_location == m_id) {
            if (m_dramctrl->available()) {
                if (m_dramctrl_work_table.count(dramctrl_msg->maddr)) {
                    m_dramctrl_req_schedule_q.erase(m_dramctrl_req_schedule_q.begin());
                    continue;
                }
                if (dramctrl_msg->dram_req->is_read()) {
                    shared_ptr<dramctrlTableEntry> new_entry(new dramctrlTableEntry);
                    new_entry->dramctrl_req = dramctrl_msg;
                    new_entry->dramctrl_rep = shared_ptr<dramctrlMsg>();
                    new_entry->per_mem_instr_stats = dramctrl_msg->per_mem_instr_stats;
                    new_entry->operation_begin_time= system_time;

                    m_dramctrl_work_table[dramctrl_msg->maddr] = new_entry;

                    if (is_remote && stats_enabled() && new_entry->per_mem_instr_stats) {
                        new_entry->per_mem_instr_stats->add_dramctrl_req_nas(system_time - dramctrl_msg->birthtime);
                    }
                    mh_log(4) << "[DRAMCTRL " << m_id << " @ " << system_time 
                              << " ] A DRAM read request on " << dramctrl_msg->maddr << " got into the table " << endl;
                } else {
                    /* if write, nothing else to do */
                    mh_log(4) << "[DRAMCTRL " << m_id << " @ " << system_time 
                              << " ] A DRAM write request on " << dramctrl_msg->maddr << " left for DRAM " << endl;
                }

                m_dramctrl->request(dramctrl_msg->dram_req);

                if (stats_enabled()) {
                    stats()->add_dram_action();
                }

                dramctrl_msg->sent = true;

                if (is_remote) {
                    m_core_receive_queues[MSG_DRAMCTRL_REQ]->pop();
                }

                m_dramctrl_req_schedule_q.erase(m_dramctrl_req_schedule_q.begin());

            } else {
                break;
            }
        } else {
            if (m_core_send_queues[MSG_DRAMCTRL_REQ]->available()) {

                shared_ptr<message_t> msg(new message_t);
                msg->src = m_id;
                msg->dst = m_dramctrl_location;
                msg->type = MSG_DRAMCTRL_REQ;
                msg->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes);
                msg->content = dramctrl_msg;

                m_core_send_queues[MSG_DRAMCTRL_REQ]->push_back(msg);

                dramctrl_msg->sent = true;

                mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] DRAMCTRL req sent " 
                          << m_id << " -> " << msg->dst << " num flits " << msg->flit_count << endl;
                m_dramctrl_req_schedule_q.erase(m_dramctrl_req_schedule_q.begin());

            } else {
                break;
            }
        }
    }
    m_dramctrl_req_schedule_q.clear();
    m_dramctrl_writeback_status.clear();

    /**********************/
    /* scheduling for CAT */
    /**********************/

    random_shuffle(m_cat_req_schedule_q.begin(), m_cat_req_schedule_q.end(), rr_fn);
    while (m_cat->available() && m_cat_req_schedule_q.size()) {
        shared_ptr<tableEntry> entry = m_cat_req_schedule_q.front();
        shared_ptr<catRequest> cat_req = entry->cat_req;

        if (cat_req->serialization_begin_time() == UINT64_MAX) {
            cat_req->set_operation_begin_time(UINT64_MAX);
        } else {
            cat_req->set_operation_begin_time(system_time);
            if (stats_enabled() && entry->per_mem_instr_stats) {
                entry->per_mem_instr_stats->get_tentative_data(T_IDX_CAT)->add_cat_srz
                    (system_time - cat_req->serialization_begin_time());
            }
        }

        m_cat->request(cat_req);

        if (stats_enabled()) {
            stats()->add_cat_action();
        }
        m_cat_req_schedule_q.erase(m_cat_req_schedule_q.begin());
    }
    m_cat_req_schedule_q.clear();

    /*********************/
    /* scheduling for L1 */
    /*********************/

    random_shuffle(m_l1_read_req_schedule_q.begin(), m_l1_read_req_schedule_q.end(), rr_fn);
    while (m_l1->read_port_available() && m_l1_read_req_schedule_q.size()) {
        shared_ptr<tableEntry>& entry = m_l1_read_req_schedule_q.front();
        shared_ptr<cacheRequest>& l1_req = entry->l1_req;

        if (l1_req->serialization_begin_time() == UINT64_MAX) {
            l1_req->set_operation_begin_time(UINT64_MAX);
        } else {
            l1_req->set_operation_begin_time(system_time);
            if (stats_enabled() && entry->per_mem_instr_stats) {
                entry->per_mem_instr_stats->get_tentative_data(T_IDX_L1)->add_l1_srz(system_time - l1_req->serialization_begin_time());
            }
        }
        if (stats_enabled()) {
            stats()->add_l1_action();
        }
        
        m_l1->request(l1_req);

        if (stats_enabled()) {
            stats()->add_l1_action();
        }
        m_l1_read_req_schedule_q.erase(m_l1_read_req_schedule_q.begin());
    }
    m_l1_read_req_schedule_q.clear();

    random_shuffle(m_l1_write_req_schedule_q.begin(), m_l1_write_req_schedule_q.end(), rr_fn);
    while (m_l1->write_port_available() && m_l1_write_req_schedule_q.size()) {
        shared_ptr<tableEntry>& entry = m_l1_write_req_schedule_q.front();
        shared_ptr<cacheRequest>& l1_req = entry->l1_req;

        if (l1_req->serialization_begin_time() == UINT64_MAX) {
            l1_req->set_operation_begin_time(UINT64_MAX);
        } else {
            l1_req->set_operation_begin_time(system_time);
            if (stats_enabled() && entry->per_mem_instr_stats) {
                entry->per_mem_instr_stats->get_tentative_data(T_IDX_L1)->add_l1_srz(system_time - l1_req->serialization_begin_time());
            }
        }
        if (stats_enabled()) {
            stats()->add_l1_action();
        }
        
        m_l1->request(l1_req);

        if (stats_enabled()) {
            stats()->add_l1_action();
        }
        m_l1_write_req_schedule_q.erase(m_l1_write_req_schedule_q.begin());
    }
    m_l1_write_req_schedule_q.clear();
   
    /*********************/
    /* scheduling for L2 */
    /*********************/

    random_shuffle(m_l2_read_req_schedule_q.begin(), m_l2_read_req_schedule_q.end(), rr_fn);
    while (m_l2->read_port_available() && m_l2_read_req_schedule_q.size()) {
        shared_ptr<tableEntry>& entry = m_l2_read_req_schedule_q.front();
        shared_ptr<cacheRequest>& l2_req = entry->l2_req;

        if (m_l2_writeback_status.count(get_start_maddr_in_line(l2_req->maddr())) > 0) {
            m_l2_read_req_schedule_q.erase(m_l2_read_req_schedule_q.begin());
            continue;
        }

        if (l2_req->serialization_begin_time() == UINT64_MAX) {
            l2_req->set_operation_begin_time(UINT64_MAX);
        } else {
            l2_req->set_operation_begin_time(system_time);
            if (stats_enabled() && entry->per_mem_instr_stats) {
                entry->per_mem_instr_stats->get_tentative_data(T_IDX_L2)->add_l2_srz(system_time - l2_req->serialization_begin_time());
            }
        }
        if (stats_enabled()) {
            stats()->add_l2_action();
        }
        
        m_l2->request(l2_req);

        if (stats_enabled()) {
            stats()->add_l2_action();
        }
        m_l2_read_req_schedule_q.erase(m_l2_read_req_schedule_q.begin());
    }
    m_l2_read_req_schedule_q.clear();

    random_shuffle(m_l2_write_req_schedule_q.begin(), m_l2_write_req_schedule_q.end(), rr_fn);
    while (m_l2->write_port_available() && m_l2_write_req_schedule_q.size()) {
        shared_ptr<tableEntry>& entry = m_l2_write_req_schedule_q.front();
        shared_ptr<cacheRequest>& l2_req = entry->l2_req;

        if (m_l2_writeback_status.count(get_start_maddr_in_line(l2_req->maddr())) > 0 &&
            m_l2_writeback_status[get_start_maddr_in_line(l2_req->maddr())] != entry)
        {
            m_l2_write_req_schedule_q.erase(m_l2_write_req_schedule_q.begin());
            continue;
        }

        if (l2_req->serialization_begin_time() == UINT64_MAX) {
            l2_req->set_operation_begin_time(UINT64_MAX);
        } else {
            l2_req->set_operation_begin_time(system_time);
            if (stats_enabled() && entry->per_mem_instr_stats) {
                entry->per_mem_instr_stats->get_tentative_data(T_IDX_L2)->add_l2_srz(system_time - l2_req->serialization_begin_time());
            }
        }
        if (stats_enabled()) {
            stats()->add_l2_action();
        }
        
        m_l2->request(l2_req);

        if (stats_enabled()) {
            stats()->add_l2_action();
        }
        m_l2_write_req_schedule_q.erase(m_l2_write_req_schedule_q.begin());
    }
    m_l2_write_req_schedule_q.clear();


    /******************************************/
    /* scheduling for sending remote requests */
    /******************************************/

    random_shuffle(m_remote_req_schedule_q.begin(), m_remote_req_schedule_q.end(), rr_fn);
    while (m_remote_req_schedule_q.size()) {
        shared_ptr<tableEntry>& entry = m_remote_req_schedule_q.front();
        shared_ptr<coherenceMsg>& remote_req = entry->remote_req;

        sharedSharedLCCMsgType_t msg_type 
            = (m_cfg.use_separate_vc_for_writes && remote_req->type == REMOTE_WRITE_REQ) ? MSG_REMOTE_WRITE_REQ : MSG_REMOTE_REQ;

        if (m_core_send_queues[msg_type]->available()) {

            shared_ptr<message_t> msg(new message_t);
            msg->src = m_id;
            msg->dst = remote_req->receiver;
            msg->type = msg_type;
            if (remote_req->type == REMOTE_READ_REQ) {
                msg->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes);
            } else {
                msg->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes + remote_req->word_count * 4);
            }
            msg->content = remote_req;

            m_core_send_queues[msg_type]->push_back(msg);

            remote_req->sent = true;

            mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] remote req sent " 
                << m_id << " -> " << msg->dst << " num flits " << msg->flit_count << endl;

            m_remote_req_schedule_q.erase(m_remote_req_schedule_q.begin());
        } else {
            break;
        }
    }
    m_remote_req_schedule_q.clear();

    /*****************************************/
    /* scheduling for sending remote replies */
    /*****************************************/

    random_shuffle(m_remote_rep_and_checkin_schedule_q.begin(), m_remote_rep_and_checkin_schedule_q.end(), rr_fn);
    while (m_remote_rep_and_checkin_schedule_q.size()) {
        bool is_remote_rep = get<0>(m_remote_rep_and_checkin_schedule_q.front());
        shared_ptr<tableEntry>& entry = get<1>(m_remote_rep_and_checkin_schedule_q.front());
        shared_ptr<coherenceMsg>& msg = (is_remote_rep)? entry->remote_rep : entry->remote_checkin;

        if (m_core_send_queues[MSG_REMOTE_REP]->available()) {

            shared_ptr<message_t> netmsg(new message_t);
            netmsg->src = m_id;
            netmsg->dst = msg->receiver;
            netmsg->type = MSG_REMOTE_REP;
            netmsg->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes + msg->word_count * 4);
            netmsg->content = msg;

            m_core_send_queues[MSG_REMOTE_REP]->push_back(netmsg);

            msg->sent = true;

            mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] ";
            if (is_remote_rep) {
                mh_log(4) << "a remote rep ";
            } else {
                mh_log(4) << "a checkin ";
            }
            mh_log(4) << "sent " << m_id << " -> " << netmsg->dst << " num flits " << netmsg->flit_count << endl;
            m_remote_rep_and_checkin_schedule_q.erase(m_remote_rep_and_checkin_schedule_q.begin());

        } else {
            break;
        }
    }
    m_remote_rep_and_checkin_schedule_q.clear();

    /*******************************************/
    /* scheduling for sending dramctrl replies */
    /*******************************************/

    random_shuffle(m_dramctrl_rep_schedule_q.begin(), m_dramctrl_rep_schedule_q.end(), rr_fn);
    while (m_dramctrl_rep_schedule_q.size()) {
        shared_ptr<dramctrlTableEntry>& entry = m_dramctrl_rep_schedule_q.front();
        shared_ptr<dramctrlMsg>& dramctrl_req = entry->dramctrl_req;
        shared_ptr<dramctrlMsg>& dramctrl_rep = entry->dramctrl_rep;

        if (dramctrl_req->sender == m_id) {
            mh_assert(m_work_table.count(dramctrl_req->maddr));
            m_work_table[dramctrl_req->maddr]->dramctrl_rep = entry->dramctrl_rep;
            mh_log(4) << "[DRAMCTRL " << m_id << " @ " << system_time << " ] has sent a DRAMCTRL rep for address " 
                      << dramctrl_rep->maddr << " to core " << m_id << endl;
            dramctrl_rep->sent = true;

        } else if (m_core_send_queues[MSG_DRAMCTRL_REP]->available()) {

            shared_ptr<message_t> msg(new message_t);
            msg->src = m_id;
            msg->dst = dramctrl_req->sender;
            msg->type = MSG_DRAMCTRL_REP;
            msg->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes + m_cfg.words_per_cache_line * 4);
            msg->content = dramctrl_rep;

            m_core_send_queues[MSG_DRAMCTRL_REP]->push_back(msg);

            dramctrl_rep->sent = true;

            mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] dramctrl rep sent " 
                      << m_id << " -> " << msg->dst << " num flits " << msg->flit_count << endl;

        }

        m_dramctrl_rep_schedule_q.erase(m_dramctrl_rep_schedule_q.begin());

    }

}

