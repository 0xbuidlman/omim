#include "../base/SRC_FIRST.hpp"

#include "../base/logging.hpp"
#include "../base/assert.hpp"

#include "commands_queue.hpp"

namespace core
{
  CommandsQueue::Environment::Environment(int threadNum)
    : m_threadNum(threadNum), m_isCancelled(false)
  {}

  void CommandsQueue::Environment::cancel()
  {
    m_isCancelled = true;
  }

  bool CommandsQueue::Environment::isCancelled() const
  {
    return m_isCancelled;
  }

  int CommandsQueue::Environment::threadNum() const
  {
    return m_threadNum;
  }

  CommandsQueue::BaseCommand::BaseCommand(bool isWaitable)
    : m_waitCount(0), m_isCompleted(false)
  {
    if (isWaitable)
      m_cond.reset(new threads::Condition());
  }

  void CommandsQueue::BaseCommand::join()
  {
    if (m_cond)
    {
      threads::ConditionGuard g(*m_cond.get());
      m_waitCount++;
      if (!m_isCompleted)
        m_cond->Wait();
    }
    else
      LOG(LERROR, ("command isn't waitable"));
  }

  void CommandsQueue::BaseCommand::finish() const
  {
    if (m_cond)
    {
      threads::ConditionGuard g(*m_cond.get());
      m_isCompleted = true;
      CHECK(m_waitCount < 2, ("only one thread could wait for the queued command"));
      if (m_waitCount)
        m_cond->Signal(true);
    }
  }

  CommandsQueue::Chain::Chain()
  {}

  void CommandsQueue::Chain::operator()(CommandsQueue::Environment const & env)
  {
    for (list<function_t>::const_iterator it = m_fns.begin(); it != m_fns.end(); ++it)
    {
      (*it)(env);

      if (env.isCancelled())
        break;
    }
  }

  CommandsQueue::Command::Command(bool isWaitable)
    : BaseCommand(isWaitable)
  {}

  void CommandsQueue::Command::perform(Environment const & env) const
  {
    m_fn(env);
    finish();
  }

  CommandsQueue::Routine::Routine(CommandsQueue * parent, int idx)
    : m_parent(parent), m_idx(idx), m_env(idx)
  {}

  void CommandsQueue::Routine::Do()
  {
    // performing initialization tasks
    for(list<shared_ptr<Command> >::const_iterator it = m_parent->m_initCommands.begin();
        it != m_parent->m_initCommands.end();
        ++it)
      (*it)->perform(m_env);

    // main loop
    while (!IsCancelled())
    {
      shared_ptr<CommandsQueue::Command> cmd = m_parent->m_commands.Front(true);

      if (m_parent->m_commands.IsCancelled())
        break;

      m_env.m_isCancelled = false;

      cmd->perform(m_env);

      m_parent->FinishCommand();
    }

    // performing finalization tasks
    for(list<shared_ptr<Command> >::const_iterator it = m_parent->m_finCommands.begin();
        it != m_parent->m_finCommands.end();
        ++it)
      (*it)->perform(m_env);
  }

  void CommandsQueue::Routine::Cancel()
  {
    m_env.cancel();

    // performing cancellation tasks
    for(list<shared_ptr<Command> >::const_iterator it = m_parent->m_cancelCommands.begin();
        it != m_parent->m_cancelCommands.end();
        ++it)
      (*it)->perform(m_env);

    IRoutine::Cancel();
  }

  void CommandsQueue::Routine::CancelCommand()
  {
    m_env.cancel();
  }

  CommandsQueue::Executor::Executor() : m_routine(0)
  {}

  void CommandsQueue::Executor::Cancel()
  {
    if (m_routine != 0)
      m_thread.Cancel();
    delete m_routine;
    m_routine = 0;
  }

  void CommandsQueue::Executor::CancelCommand()
  {
    m_routine->CancelCommand();
  }

  CommandsQueue::CommandsQueue(size_t executorsCount)
    : m_executors(new Executor[executorsCount]), m_executorsCount(executorsCount),
      m_activeCommands(0)
  {
  }

  CommandsQueue::~CommandsQueue()
  {
    // @TODO memory leak in m_executors? call Cancel()?
  }

  void CommandsQueue::Cancel()
  {
    m_commands.Cancel();

    for (size_t i = 0; i < m_executorsCount; ++i)
      m_executors[i].Cancel();

    delete [] m_executors;
    m_executors = 0;
  }

  void CommandsQueue::CancelCommands()
  {
    for (size_t i = 0; i < m_executorsCount; ++i)
      m_executors[i].CancelCommand();
  }

  void CommandsQueue::Start()
  {
    for (size_t i = 0; i < m_executorsCount; ++i)
    {
      m_executors[i].m_routine = new CommandsQueue::Routine(this, i);
      m_executors[i].m_thread.Create(m_executors[i].m_routine);
    }
  }

  void CommandsQueue::AddCommand(shared_ptr<Command> const & cmd)
  {
    threads::ConditionGuard g(m_cond);
    m_commands.PushBack(cmd);
    ++m_activeCommands;
  }

  void CommandsQueue::AddInitCommand(shared_ptr<Command> const & cmd)
  {
    m_initCommands.push_back(cmd);
  }

  void CommandsQueue::AddFinCommand(shared_ptr<Command> const & cmd)
  {
    m_finCommands.push_back(cmd);
  }

  void CommandsQueue::AddCancelCommand(shared_ptr<Command> const & cmd)
  {
    m_cancelCommands.push_back(cmd);
  }

  void CommandsQueue::FinishCommand()
  {
    threads::ConditionGuard g(m_cond);

    --m_activeCommands;
    if (m_activeCommands == 0)
      m_cond.Signal();
  }

  void CommandsQueue::Join()
  {
    threads::ConditionGuard g(m_cond);
    if (m_activeCommands != 0)
      m_cond.Wait();
  }

  void CommandsQueue::Clear()
  {
    m_commands.Clear();
  }

  int CommandsQueue::ExecutorsCount() const
  {
    return m_executorsCount;
  }
}
