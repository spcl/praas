#!/usr/bin/env python

import base64
from dataclasses import dataclass
import logging
from typing import Optional

import boto3
import click
from dataclasses_json import dataclass_json
import docker

TASK_ASSUMED_ROLE = """{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Action": "sts:AssumeRole",
        "Principal": {
          "Service": "ecs-tasks.amazonaws.com"
        },
        "Effect": "Allow",
        "Sid": ""
    }
  ]
}
"""

TASK_S3_ROLE = """{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": "s3:*",
      "Resource": "arn:aws:s3:::praas/*"
    }
  ]
}
"""

EXECUTION_ROLE ="""{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Action": [
                "ecr:GetAuthorizationToken",
                "ecr:BatchCheckLayerAvailability",
                "ecr:GetDownloadUrlForLayer",
                "ecr:BatchGetImage",
                "logs:CreateLogStream",
                "logs:PutLogEvents"
            ],
            "Resource": "*",
            "Effect": "Allow"
        }
    ]
}
"""

TASK_DEFINITION = {
  "family": "",
  "networkMode": "awsvpc",
  "taskRoleArn": "", 
  "containerDefinitions": [
      {
          "name": "process",
          "image": "",
          "portMappings": [
              {
                  "containerPort": 8000,
                  "hostPort": 8000,
                  "protocol": "tcp"
              }
          ],
          "essential": True
      }
  ],
  "requiresCompatibilities": [
      "FARGATE"
  ],
  "cpu": "256",
  "memory": "512"
}

@dataclass_json
@dataclass
class FargateDeployment:
    cluster_name: Optional[str] = None
    cluster_arn: Optional[str] = None

@click.group()
def cli():
    pass

@cli.group()
def cluster():
    pass

@cluster.command()
@click.argument('name', type=str)
@click.option('--region', type=str, default='us-east-1')
def create(name: str, region: str):

    client = boto3.client('ecs', region_name=region)
    res = client.create_cluster(
        clusterName=name
    )
    logging.info(f"Created cluster {name}, ARN {res['cluster']['clusterArn']}")

@cli.group()
def role():
    pass

@role.command("create-task-role")
@click.argument('name', type=str)
@click.option('--region', type=str, default='us-east-1')
def create_role(name: str, region: str):

    client = boto3.client('iam', region_name=region)
    res = client.create_role(RoleName=name, AssumeRolePolicyDocument=TASK_ASSUMED_ROLE)
    arn = res['Role']['Arn']
    res = client.put_role_policy(RoleName=name, PolicyName=f"{name}-s3", PolicyDocument=TASK_S3_ROLE)
    logging.info(f"Created role {name}, ARN {arn}")

@role.command("create-execution-role")
@click.argument('name', type=str)
@click.option('--region', type=str, default='us-east-1')
def create_execution_role(name: str, region: str):

    client = boto3.client('iam', region_name=region)
    res = client.create_role(RoleName=name, AssumeRolePolicyDocument=TASK_ASSUMED_ROLE)
    arn = res['Role']['Arn']
    res = client.put_role_policy(RoleName=name, PolicyName=f"{name}-exec", PolicyDocument=EXECUTION_ROLE)
    logging.info(f"Created execution role {name}, ARN {arn}")

@cli.group()
def container():
    pass

@container.command("create-registry")
@click.argument('name', type=str)
@click.option('--region', type=str, default='us-east-1')
def create_registry(name: str, region: str):

    client = boto3.client('ecr', region_name=region)
    res = client.create_repository(repositoryName=name)
    logging.info(f"Created registry{name}, ID {registryId}, ARN {arn}")

@container.command("push")
@click.argument('registry-uri', type=str)
@click.argument('registry-id', type=str)
@click.argument('repository', type=str)
@click.argument('image', type=str)
@click.option('--region', type=str, default='us-east-1')
def push_image(registry_uri: str, registry_id: str, repository:str, image: str, region: str):

    client = boto3.client('ecr', region_name=region)
    res = client.get_authorization_token(registryIds=[registry_id])
    token = res['authorizationData'][0]['authorizationToken']
    token = base64.b64decode(token).decode()
    _, token = token.split(':')

    _, tag = image.split(':')
    docker_client = docker.from_env()
    docker_client.login(username='AWS', password=token, registry=registry_uri)
    docker_client.images.get(image).tag(repository=f"{registry_uri}/{repository}", tag=tag)

@cli.group()
def task():
    pass

@task.command("create")
@click.argument('name', type=str)
@click.argument('container', type=str)
@click.argument('task-role-arn', type=str)
@click.argument('exec-role-arn', type=str)
@click.option('--region', type=str, default='us-east-1')
def create_task(name: str, container: str, task_role_arn: str, exec_role_arn: str, region: str):

    client = boto3.client('ecs', region_name=region)
    TASK_DEFINITION['family'] = name
    TASK_DEFINITION['taskRoleArn'] = task_role_arn
    TASK_DEFINITION['executionRoleArn'] = exec_role_arn
    TASK_DEFINITION['containerDefinitions'][0]['image'] = container
    res = client.register_task_definition(
        **TASK_DEFINITION
    )
    print(res)

if __name__ == '__main__':
    logging.basicConfig()
    cli()

