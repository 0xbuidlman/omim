//
//  MWMLocationButton.h
//  Maps
//
//  Created by Ilya Grechuhin on 14.05.15.
//  Copyright (c) 2015 MapsWithMe. All rights reserved.
//

#import <Foundation/Foundation.h>

#include "platform/location.hpp"

@interface MWMLocationButton : NSObject

@property (nonatomic) BOOL hidden;

- (instancetype)init __attribute__((unavailable("init is not available")));
- (instancetype)initWithParentView:(UIView *)view;
- (void)setMyPositionMode:(location::EMyPositionMode)mode;

@end
