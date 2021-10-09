/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef CHARGER_H
#define CHARGER_H
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/node.h"
#include "ns3/vector.h"
#include "ns3/timer.h"
#include "ns3/wsngr.h"
#include "ns3/singleton.h"
#include <vector>
#include <set>
#include<iostream>
#include<unordered_map>
namespace ns3 {
namespace charger{

constexpr double ALPHA = 0.01;

struct ChargerBase{

    enum State{
        IDLE,CHARGING,SELF_CHARGING,MOVING
    };
    constexpr static double CHARGING_RATE = 0.005 * 10;
    constexpr static double MOVING_RATE = 5;
    constexpr static double MOVING_ENERGY = 0.6;
    constexpr static double MAX_ENERGY = 4000;
    static Time SELF_CHARGING_TIME;

    Vector position;
    double energy = MAX_ENERGY;

    State state{IDLE};

    Ipv4Address working_node;
    Timer checkTimer;
    Timer eventTimer;

    static int dead_node;
    static double energy_in_moving;
    static double energy_for_nodes;
    static int charged_sensor;
    static int requested_sensor;
    static double tot_latency;
    // static Time first_dead;

    struct ChargeCmp{
        bool operator()(const Ipv4Address& a,const Ipv4Address& b){
            auto& x = wsngr::RoutingProtocol::nodes[a];
            auto& y = wsngr::RoutingProtocol::nodes[b];
            auto charger = Singleton<ChargerBase>::Get();

            return CalculateDistance(x.position,charger->position) < CalculateDistance(y.position,charger->position);
        }
    };

    std::set<Ipv4Address> chargeQueue;

    ChargerBase();

    double getChargingEfficiency(double distance);
    Time getMovingTime(double distance);
    Time getChargingTime(double energy);
    void handle();
    void checkHandle();
    void run();

    void print_statistics();
};

// ChargerBase& getChargeBase();


}
}

#endif /* CHARGER_H */
