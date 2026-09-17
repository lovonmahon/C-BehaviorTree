#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <functional>

enum class Status
{
    Success,
    Failure
};

class Node
{
public:
    virtual Status run() = 0;
    virtual ~Node() = default;
};

/// This action guarantees success
class Action : public Node
{
public:
    explicit Action(const std::string& name) : m_name(name){};
    Status run() override
    {
        std::cout << "Performing: " << m_name << std::endl;
        return Status::Success;
    }
private:
    std::string m_name;
};

/// Return true of false given the condition
class ConditionalAction : public Node
{
public:
    ConditionalAction(const std::string& name, std::function<bool()> condition) : m_name(name), m_conditionFunc(condition){};
    Status run() override
    {
        if(!m_conditionFunc())
        {
            std::cout << m_name << " failed." <<"\n";
            return Status::Failure;
        }
        while(m_conditionFunc())
        {
            std::cout << "Trying: " << m_name << "\n";
            std::cout << m_name << " succeeeded!\n";
            return Status::Success;
        }
        return Status::Failure;
    }
private:
    std::string m_name;
    std::function<bool()> m_conditionFunc;
};

/// Sequence: 
/// runs the child nodes in order.  Any child failure stops the entire sequence
class Sequence : public Node
{
public:
    void add_child(std::unique_ptr<Node> child)
    {
        m_childrenVec.push_back(std::move(child));
    }
    Status run() override
    {
        for(std::unique_ptr<Node>& child : m_childrenVec)
        {
            if( child->run() == Status::Failure)
            {
                return Status::Failure;
            }
        }
        return Status::Success;
    }
private:
    std::vector<std::unique_ptr<Node>> m_childrenVec;
};


/// Conditional sequence: Returns true or false based on given conditions
class ConditionalSequence : public Node
{
public:
    explicit ConditionalSequence(const std::string& name, std::function<bool()> condition) : m_name(name), m_conditionFunc(condition){};
    void add_child(std::unique_ptr<Node> child)
    {
        m_childrenVec.push_back(std::move(child));
    }
    Status run() override
    {
        bool ok = false;
        m_index = 0;
        if(!m_conditionFunc())
        {
            // reset index count
            m_index = 0;
            return Status::Failure;
        }
        while(m_index < m_childrenVec.size())
        {
            Status childStatus = m_childrenVec[m_index]->run();
            if(childStatus == Status::Success)
            {
                m_index++;
                std::cout << m_name << " success\n";
            }
            if(childStatus == Status::Failure)
            {
                // reset index and back out
                m_index = 0;
                std::cout << m_name << " failure\n";
                return Status::Failure;
            }
        }
        std::cout << m_name << std::endl;
        return ok ? Status::Success : Status::Failure;
    }
private:
    std::string m_name;
    std::function<bool()> m_conditionFunc;
    std::vector<std::unique_ptr<Node>> m_childrenVec;
    size_t m_index;
};

/// The selector class chooses branches in order and runs until success or failure, then re-evaluates as needed
class Selector : public Node
{
public:
    void add_child(std::unique_ptr<Node> child)
    {
        m_childrenVec.push_back(std::move(child));
    }
    Status run() override
    {
        for(std::unique_ptr<Node>& child : m_childrenVec)
        {
            Status childNodeStatus = child->run();
            if( childNodeStatus == Status::Success)
            {
                // keep ticking each child branch
                return Status::Success;
            }
        }
        return Status::Failure;
    }
private:
    std::vector<std::unique_ptr<Node>> m_childrenVec;
};

/// The entrance node runs once per tick
/// and ticks the child node(main selector node)
///  This tick cascades down to all nodes down the hierarchy
class Entrance : public Node
{
public:
    explicit Entrance(std::unique_ptr<Node> child) : m_child(std::move(child)){};
    Status run() override
    {
        std::cout << "\e[0;32m===== Entering Behavior Tree =====\e[0m\n";
        return m_child->run();
    }
private:
    std::unique_ptr<Node> m_child;
};

/// Repeater node loops forever.  This is the "tick" node
class Repeater : public Node
{
public:
    explicit Repeater(std::unique_ptr<Node> child, int simTimeInSeconds, int tickInMilliseconds) : m_child(std::move(child)), m_simSeconds(simTimeInSeconds), m_tickIntervalMs(tickInMilliseconds){};
    Status run() override
    {
        
        using std::chrono::steady_clock;
        using std::chrono::seconds;
        using std::chrono::milliseconds;

        
        steady_clock::time_point startTime = steady_clock::now();
        steady_clock::time_point endTime   = startTime + seconds(m_simSeconds);

        while(steady_clock::now() < endTime)
        {
            m_child->run();
            std::this_thread::sleep_for(milliseconds(m_tickIntervalMs));
        }
        return Status::Success;
    }
    /// for switching to a different ticker 
    std::unique_ptr<Node> release_child() 
    {
        /// transfer ownership of the child node to a different repeater for ticking when needed
        return std::move(m_child); 
    }
private:
    std::unique_ptr<Node> m_child;
    int m_simSeconds; // seconds
    int m_tickIntervalMs; // milliseconds
};

/// driver
int main()
{
    bool bearVisible = false;
    auto keepExploring = [&bearVisible](){ return !bearVisible;};
    auto prepareToDefend = [&bearVisible](){ return bearVisible;};

    /// Peaceful actions
    std::unique_ptr<ConditionalSequence> conditionalSequenceOne = std::make_unique<ConditionalSequence>("Go for a walk", keepExploring);
    conditionalSequenceOne->add_child(std::make_unique<ConditionalAction>("Smell the flowers", keepExploring));
    conditionalSequenceOne->add_child(std::make_unique<ConditionalAction>("Sit under a tree", keepExploring));
    conditionalSequenceOne->add_child(std::make_unique<ConditionalAction>("Look at the birds", keepExploring));
    conditionalSequenceOne->add_child(std::make_unique<ConditionalAction>("Feel happy", keepExploring));

    // Defense actions
    std::unique_ptr<ConditionalSequence> conditionalSequenceTwo = std::make_unique<ConditionalSequence>("Prepare to defend!", prepareToDefend);
    conditionalSequenceTwo->add_child(std::make_unique<ConditionalAction>("equip bear spray", prepareToDefend));
    conditionalSequenceTwo->add_child(std::make_unique<ConditionalAction>("Deploy bear spray", prepareToDefend));

    /// sub selectors
    std::unique_ptr<Selector> subSelectorOne = std::make_unique<Selector>();
    std::unique_ptr<Selector> subSelectorTwo = std::make_unique<Selector>();

    subSelectorOne->add_child(std::move(conditionalSequenceOne));
    subSelectorTwo->add_child(std::move(conditionalSequenceTwo));

    /// main selector
    std::unique_ptr<Selector> mainselector = std::make_unique<Selector>();
    mainselector->add_child(std::move(subSelectorOne));
    mainselector->add_child(std::move(subSelectorTwo));

    /// Entrance node
    std::unique_ptr<Entrance> entranceNode = std::make_unique<Entrance>(std::move(mainselector));

    /// Run for 5 seconds at 10 ticks per second
    int simSeconds = 5;
    /// Tick 0.1 seconds(100 ms)
    int tickIntervalMs = 1000;
    std::unique_ptr<Repeater> repeaterNode  = std::make_unique<Repeater>(std::move(entranceNode), simSeconds, tickIntervalMs);

    repeaterNode->run();

    bearVisible = true;

    int extendedSimSeconds = 1;
    std::unique_ptr<Repeater> shortRepeaterNode  = std::make_unique<Repeater>(repeaterNode->release_child(), extendedSimSeconds, tickIntervalMs);
    shortRepeaterNode->run();
    std::cout << "Running program.\n";

    return 0;
}


