#!/usr/bin/env python

import base64
from dataclasses import dataclass, field
import logging
import json
from typing import List, Optional

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
                "logs:CreateLogGroup",
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
            "essential": True,
            "logConfiguration": {
                "logDriver": "awslogs",
                "options": {
                    "awslogs-create-group": "true",
                    "awslogs-group": "",
                    "awslogs-region": "us-east-1",
                    "awslogs-stream-prefix": "ecs"
                }
            }
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
class FargateTask:
    name: str
    container: str
    arn: str

@dataclass_json
@dataclass
class FargateDeployment:
    region: str
    subnet: str
    images: List[str] = field(default_factory=list)
    tasks: List[FargateTask] = field(default_factory=list)

    cluster_name: Optional[str] = None
    cluster_arn: Optional[str] = None

    task_role_name: Optional[str] = None
    task_role_arn: Optional[str] = None

    execution_role_name: Optional[str] = None
    execution_role_arn: Optional[str] = None

    ecr_registry_name: Optional[str] = None
    ecr_registry_id: Optional[str] = None
    ecr_registry_uri: Optional[str] = None
    ecr_authorization_token: Optional[str] = None

def load_deployment(input_config: str) -> FargateDeployment:

    with open(input_config, 'r') as in_file:
        return FargateDeployment.from_json(in_file.read())

def save_deployment(config: FargateDeployment, output_config: str):

    with open(output_config, 'w') as out_file:
        json.dump(config.to_dict(), out_file, indent=2)

@click.group()
def cli():
    pass

@cli.group()
def cluster():
    pass

@cluster.command("create")
@click.argument('name', type=str)
@click.argument('input_config', type=str)
@click.argument('output_config', type=str)
def create_cluster(name: str, input_config: str, output_config: str):

    deployment = load_deployment(input_config)

    client = boto3.client('ecs', region_name=deployment.region)
    res = client.create_cluster(
        clusterName=name
    )
    deployment.cluster_name = name
    deployment.cluster_arn = res['cluster']['clusterArn']
    logging.info(f"Created cluster {name}, ARN {deployment.cluster_arn}")

    save_deployment(deployment, output_config)

@cli.group()
def role():
    pass

@role.command("create")
@click.argument('name', type=str)
@click.argument('input_config', type=str)
@click.argument('output_config', type=str)
def create_role(name: str, input_config: str, output_config: str):

    deployment = load_deployment(input_config)

    client = boto3.client('iam', region_name=deployment.region)
    role_name = f"{name}-task-role"
    res = client.create_role(RoleName=role_name, AssumeRolePolicyDocument=TASK_ASSUMED_ROLE)
    arn = res['Role']['Arn']
    res = client.put_role_policy(RoleName=role_name, PolicyName=f"{name}-s3", PolicyDocument=TASK_S3_ROLE)
    logging.info(f"Created task role {role_name}, ARN {arn}")
    deployment.task_role_arn = arn
    deployment.task_role_name = role_name

    role_name = f"{name}-exec-role"
    res = client.create_role(RoleName=role_name, AssumeRolePolicyDocument=TASK_ASSUMED_ROLE)
    arn = res['Role']['Arn']
    res = client.put_role_policy(RoleName=role_name, PolicyName=f"{name}-exec", PolicyDocument=EXECUTION_ROLE)
    logging.info(f"Created execution role {role_name}, ARN {arn}")
    deployment.execution_role_arn = arn
    deployment.execution_role_name = role_name

    save_deployment(deployment, output_config)

@cli.group()
def container():
    pass

@container.command("create-registry")
@click.argument('name', type=str)
@click.argument('input_config', type=str)
@click.argument('output_config', type=str)
def create_registry(name: str, input_config: str, output_config: str):

    deployment = load_deployment(input_config)

    client = boto3.client('ecr', region_name=deployment.region)
    res = client.create_repository(repositoryName=name)
    deployment.ecr_registry_name = name
    deployment.ecr_registry_id = res['repository']['registryId']
    deployment.ecr_registry_uri = res['repository']['repositoryUri']

    save_deployment(deployment, output_config)

@container.command("push")
@click.argument('image', type=str)
@click.argument('input_config', type=str)
@click.argument('output_config', type=str)
@click.option('--regenerate-token', is_flag=True)
def push_image(image: str, input_config: str, output_config: str, regenerate_token: bool):

    deployment = load_deployment(input_config)

    docker_client = docker.from_env()
    if deployment.ecr_authorization_token is None or regenerate_token:
        client = boto3.client('ecr', region_name=deployment.region)
        res = client.get_authorization_token(registryIds=[deployment.ecr_registry_id])
        token = res['authorizationData'][0]['authorizationToken']
        token = base64.b64decode(token).decode()
        _, deployment.ecr_authorization_token = token.split(':')

    _, tag = image.split(':')
    uri = deployment.ecr_registry_uri.split('/')[0]
    # https://github.com/docker/docker-py/issues/2256
    docker_client.login(username='AWS', password=deployment.ecr_authorization_token, registry=uri, reauth=True)
    repo_path = deployment.ecr_registry_uri
    docker_client.images.get(image).tag(repository=repo_path, tag=tag)

    # last line is empty
    output = docker_client.images.push(repository=repo_path, tag=tag).split('\n')
    last_line = json.loads(output[-2])
    if 'error' in last_line:
        logging.error(f"Failed pushing! Reasong: {last_line['error']}") 

    if tag not in deployment.images:
        deployment.images.append(tag)

    save_deployment(deployment, output_config)

@cli.group()
def task():
    pass

@task.command("create")
@click.argument('name', type=str)
@click.argument('image_idx', type=int)
@click.argument('input_config', type=str)
@click.argument('output_config', type=str)
def create_task(name: str, image_idx: int, input_config: str, output_config: str):

    deployment = load_deployment(input_config)

    client = boto3.client('ecs', region_name=deployment.region)
    TASK_DEFINITION['family'] = name
    TASK_DEFINITION['taskRoleArn'] = deployment.task_role_arn
    TASK_DEFINITION['executionRoleArn'] = deployment.execution_role_arn
    image_tag = deployment.images[image_idx]
    image_name = f"{deployment.ecr_registry_uri}:{image_tag}"
    TASK_DEFINITION['containerDefinitions'][0]['image'] = image_name
    TASK_DEFINITION['containerDefinitions'][0]['logConfiguration']['options']['awslogs-group'] = f"/ecs/{name}"

    res = client.register_task_definition(
        **TASK_DEFINITION
    )
    arn = res['taskDefinition']['taskDefinitionArn']
    deployment.tasks.append(FargateTask(name, image_name, arn))

    save_deployment(deployment, output_config)

if __name__ == '__main__':
    logging.basicConfig()
    cli()

