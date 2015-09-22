//
// Created by Igor Glotov on 23/07/14.
// Copyright (c) 2014 Mailru Group. All rights reserved.
//

#import <Foundation/Foundation.h>


@interface MRAbstractDataProvider : NSObject


- (void)collectData;

- (NSDictionary *)data;

- (void)addParam:(NSString *)value forKey:(NSString *)key;

- (BOOL)hasData;

- (NSUInteger)dataCount;
@end