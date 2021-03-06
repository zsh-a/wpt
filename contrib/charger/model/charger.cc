/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "charger.h"

namespace ns3 {
namespace charger{


Time ChargerBase::SELF_CHARGING_TIME = Seconds(1800);

int ChargerBase::return_sink = 0;
int ChargerBase::dead_node = 0;
double ChargerBase::energy_in_moving = 0;
double ChargerBase::energy_for_nodes = 0;
int ChargerBase::charged_sensor = 0;
int ChargerBase::requested_sensor = 0;
double ChargerBase::tot_latency = 0;
Time ChargerBase::first_request_time = Time::Max();

ChargerBase::ChargerBase():
    eventTimer(Timer::CANCEL_ON_DESTROY),
    checkTimer(Timer::CANCEL_ON_DESTROY)
{
    auto& sink = wsngr::RoutingProtocol::getSink();
    position = sink.position;

}


double ChargerBase::getChargingEfficiency(double distance){
    return 0.8;
}
void ChargerBase::handle(){
    std::cout << "state : " << state << "\n";
    switch(state){
        case IDLE:{
            if(!chargeQueue.size()) return;

            // charge self
            auto& sink = wsngr::RoutingProtocol::getSink();

            auto q = std::set(chargeQueue.begin(),chargeQueue.end(),ChargerCmp{});
            
            auto ip = *q.begin();

            auto& node = wsngr::RoutingProtocol::nodes[ip];
            double dist = CalculateDistance(node.position,position);

            double tot_dist = dist + CalculateDistance(node.position,sink.position);

            double moveing_to_sink_energy = tot_dist * MOVING_ENERGY + (wsngr::NodeInfo::MAX_ENERGY - node.energy) / getChargingEfficiency(0);
            std::cout << " " << moveing_to_sink_energy << " ip " << ip << "\n";
            if(energy < moveing_to_sink_energy){
                state = SELF_CHARGING;
                Time time = getMovingTime(CalculateDistance(position,sink.position)) + SELF_CHARGING_TIME;
                // === record
                energy_in_moving += CalculateDistance(position,sink.position) * MOVING_ENERGY;

                // ===
                energy = MAX_ENERGY;
                eventTimer.Schedule(time);
                return;
            }
            Time time = getMovingTime(dist);
            // === record
            energy_in_moving += dist * MOVING_ENERGY;
            // ===
            state = MOVING;
            working_node = ip;
            eventTimer.Schedule(time);
        }
        break;
        case CHARGING:{
            state = IDLE;
            auto& node = wsngr::RoutingProtocol::nodes[working_node];
            chargeQueue.erase(working_node);
            node.state = wsngr::NodeState::WORKING;
            node.energy_consume_in_record_intervel = 0;
            node.last_update_time = Simulator::Now();
            // === record
            tot_latency += (Simulator::Now() - node.requested_time).GetSeconds();
            // ===
            eventTimer.Schedule(Seconds(0.1));
        }
        break;
        case SELF_CHARGING:{
            state = IDLE;
            auto& sink = wsngr::RoutingProtocol::getSink();
            return_sink++;
            position = sink.position;
        }
        break;
        case MOVING:{
            state = CHARGING;
            auto& node = wsngr::RoutingProtocol::nodes[working_node];
            position = node.position;
            Time time = getChargingTime(node.energy);

            energy -= (wsngr::NodeInfo::MAX_ENERGY - node.energy) / getChargingEfficiency(0);

            // === record
            energy_for_nodes += wsngr::NodeInfo::MAX_ENERGY - node.energy;
            charged_sensor++;
            // ===

            node.state = wsngr::NodeState::CHARGING;
            node.energy = wsngr::NodeInfo::MAX_ENERGY;

            eventTimer.Schedule(time);
        }
        break;
    }
}

Time ChargerBase::getMovingTime(double distance){
    return Seconds(distance / MOVING_RATE);
}

Time ChargerBase::getChargingTime(double energy){
    return Seconds((wsngr::NodeInfo::MAX_ENERGY - energy) / getChargingEfficiency(0) / CHARGING_RATE );
}

void ChargerBase::checkHandle(){

    auto& nodes = wsngr::RoutingProtocol::GetNodes();
    for(auto& [ip,info] : nodes){
        if(info.energy <= wsngr::NodeInfo::CHARING_THRESHOLD){
            if(!chargeQueue.count(ip)){
                info.requested_time = Simulator::Now();
                if(ChargerBase::first_request_time == Time::Max()) [[unlikely]]
                    ChargerBase::first_request_time = info.requested_time;
                requested_sensor++;
                chargeQueue.insert(ip);
            }
        }
    }
    if(chargeQueue.size() && state == IDLE && eventTimer.IsExpired()){
        eventTimer.Schedule(Seconds(0.1));
    }
    checkTimer.Schedule(Seconds(20));
}


void ChargerBase::run(){

    eventTimer.SetFunction(&ChargerBase::handle,this);
    checkTimer.SetFunction(&ChargerBase::checkHandle,this);

    checkTimer.Schedule(Seconds(20));
    eventTimer.Schedule(Seconds(5));
}

void ChargerBase::print_statistics(){

    auto& nodes = wsngr::RoutingProtocol::GetNodes();
    for(auto& [ip,info] : nodes){
        dead_node += info.deadTimes;
    }
    std::cout << "======statistics======\n"
        << "return sink times : " << return_sink << " \n"
        << "dead nodes : " << dead_node << " \n"
        << "energy in moving : " << energy_in_moving << " \n"
        << "nodes receive energy : " << energy_for_nodes << "\n"
        << "energy usage efficiency: " << energy_for_nodes / (energy_for_nodes / getChargingEfficiency(0) + energy_in_moving) << "\n"
        << "charged sensors: " << charged_sensor << "\n"
        << "requested sensors: " << requested_sensor << "\n"
        << "survival rate: " << charged_sensor*1.0 / requested_sensor << "\n"
        << "average charging latency: " << tot_latency / charged_sensor << "s\n"
        << "first request time : " << first_request_time.GetSeconds() << "s\n"
        << "======================";
}

}
}

