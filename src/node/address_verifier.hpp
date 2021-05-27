#pragma once

#ifndef _node_address_verifier_hpp
#define _node_address_verifier_hpp

#include "connection.hpp"

namespace epilog { namespace node {

class task_address_verifier : public out_task {
public:
    task_address_verifier(out_connection *out);

private:
    void process_version(const common::term ver);
    virtual void process() override;
};

}}

#endif


