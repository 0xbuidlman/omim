#pragma once

#include "drape/pointers.hpp"

#include "geometry/point2d.hpp"

#include "std/unordered_set.hpp"

namespace df
{

class Animation
{
public:
  enum Type
  {
    Sequence,
    Parallel,
    MapLinear,
    MapScale,
    MapFollow,
    Arrow,
    KineticScroll
  };

  enum Object
  {
    MyPositionArrow,
    MapPlane,
    Selection
  };

  enum ObjectProperty
  {
    Position,
    Scale,
    Angle
  };

  struct PropertyValue
  {
    enum Type
    {
      ValueD,
      ValuePointD
    };

    PropertyValue()
    {}

    explicit PropertyValue(double value)
      : m_type(ValueD)
      , m_valueD(value)
    {}

    explicit PropertyValue(m2::PointD const & value)
      : m_type(ValuePointD)
      , m_valuePointD(value)
    {}

    Type m_type;
    union
    {
      m2::PointD m_valuePointD;
      double m_valueD;
    };
  };

  using TObject = uint32_t;
  using TProperty = uint32_t;
  using TAnimObjects = unordered_set<TObject>;
  using TObjectProperties = unordered_set<TProperty>;
  using TAction = function<void(ref_ptr<Animation>)>;

  Animation(bool couldBeInterrupted, bool couldBeBlended)
    : m_couldBeInterrupted(couldBeInterrupted)
    , m_couldBeBlended(couldBeBlended)
    , m_interruptedOnCombine(false)
    , m_couldBeRewinded(true)
  {}

  virtual void OnStart() { if (m_onStartAction != nullptr) m_onStartAction(this); }
  virtual void OnFinish() { if (m_onFinishAction != nullptr) m_onFinishAction(this); }
  virtual void Interrupt() { if (m_onInterruptAction != nullptr) m_onInterruptAction(this); }

  virtual Type GetType() const = 0;
  virtual string GetCustomType() const { return string(); }

  virtual TAnimObjects const & GetObjects() const = 0;
  virtual bool HasObject(TObject object) const = 0;
  virtual TObjectProperties const & GetProperties(TObject object) const = 0;
  virtual bool HasProperty(TObject object, TProperty property) const = 0;
  virtual bool HasTargetProperty(TObject object, TProperty property) const;

  virtual void SetMaxDuration(double maxDuration) = 0;
  virtual double GetDuration() const = 0;
  virtual bool IsFinished() const = 0;

  virtual void Advance(double elapsedSeconds) = 0;
  virtual void Finish() { OnFinish(); }

  virtual bool GetProperty(TObject object, TProperty property, PropertyValue & value) const = 0;
  virtual bool GetTargetProperty(TObject object, TProperty property, PropertyValue & value) const = 0;

  void SetOnStartAction(TAction const & action) { m_onStartAction = action; }
  void SetOnFinishAction(TAction const & action) { m_onFinishAction = action; }
  void SetOnInterruptAction(TAction const & action) { m_onInterruptAction = action; }

  bool CouldBeBlended() const { return m_couldBeBlended; }
  bool CouldBeInterrupted() const { return m_couldBeInterrupted; }
  bool CouldBeBlendedWith(Animation const & animation) const;

  void SetInterruptedOnCombine(bool enable) { m_interruptedOnCombine = enable; }
  bool GetInterruptedOnCombine() const { return m_interruptedOnCombine; }

  void SetCouldBeInterrupted(bool enable) { m_couldBeInterrupted = enable; }
  void SetCouldBeBlended(bool enable) { m_couldBeBlended = enable; }
  
  void SetCouldBeRewinded(bool enable) { m_couldBeRewinded = enable; }
  bool CouldBeRewinded() const { return m_couldBeRewinded; }

protected:
  TAction m_onStartAction;
  TAction m_onFinishAction;
  TAction m_onInterruptAction;

  // Animation could be interrupted in case of blending impossibility.
  bool m_couldBeInterrupted;
  // Animation could be blended with other animations.
  bool m_couldBeBlended;
  // Animation must be interrupted in case of combining another animation.
  bool m_interruptedOnCombine;
  // Animation could be rewinded in case of finishing.
  bool m_couldBeRewinded;
};

} // namespace df

